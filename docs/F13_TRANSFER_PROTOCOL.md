# Custom CAN Transfer Test Protocol

This non-destructive protocol verifies the complete PC-to-MCU-host-to-target
CAN path before firmware-update work is attempted. It never erases or writes
flash. `RL78_F13_CAN_Transfer` and `RL78_F24_CAN_Transfer` are independent
device projects implementing this contract. The PC workflow is named
`Custom Transfer` and accepts matching implementations on other MCUs.

Custom Transfer does not load or parse a target firmware BIN. Program the
diagnostic target with its normal debugger/programmer, then use the tool to
discover it and run the transfer test. The `Load Update Image` action is only
for metadata-bearing CAN Upgrade application images.

## 1. CAN configuration

- Classical CAN: 500 kbit/s.
- Request ID: standard data frame `0x710`.
- Response ID: standard data frame `0x711`.
- DLC: exactly 8.
- F13 reference hardware receive rule: ID `0x710`, mask `0x7FF`, standard data
  only.
- MCU-host receive rule: ID `0x711`, mask `0x7FF`, standard data only.

Extended, remote, wrong-ID, and non-8-byte frames are not valid protocol
requests. The exact F13 hardware rule rejects the first three classes before
protocol dispatch.

## 2. Request and response format

Every request and response is exactly eight bytes.

| Byte | Request | Successful response |
| ---: | --- | --- |
| 0 | Request magic `0xA5` | Response magic `0x5A` |
| 1 | Protocol major `0x01` | Protocol major `0x01` |
| 2 | Non-zero sequence | Matching sequence |
| 3 | Command | Matching command |
| 4..6 | Three command bytes | Three response bytes |
| 7 | CRC-8 over bytes 0..6 | CRC-8 over bytes 0..6 |

CRC parameters:

- Name: CRC-8/ATM (CRC-8/SMBUS polynomial form).
- Polynomial: `0x07`.
- Initial value: `0x00`.
- RefIn/RefOut: false.
- XorOut: `0x00`.
- Check value for ASCII `123456789`: `0xF4`.

## 3. Error response

An error response uses command `0x7F`:

| Byte | Meaning |
| ---: | --- |
| 4 | Original request command |
| 5 | Error status |
| 6 | Optional detail |

| Status | Value | Meaning |
| --- | ---: | --- |
| `BAD_FORMAT` | `0x01` | Frame layout is invalid |
| `BAD_MAGIC` | `0x02` | Request magic is not `0xA5` |
| `BAD_VERSION` | `0x03` | Protocol major is unsupported |
| `BAD_SEQUENCE` | `0x04` | Sequence is zero or reused with changed content |
| `CRC_ERROR` | `0x05` | Request CRC-8 is invalid |
| `BAD_COMMAND` | `0x06` | Command is unsupported |
| `BAD_ARGUMENT` | `0x07` | Command payload is invalid |

A frame with the wrong CAN type, ID, or DLC may be rejected without a response
because its sequence and command cannot be treated as a valid request.

## 4. Commands

| Command | Value | Request bytes 4..6 | Successful response bytes 4..6 |
| --- | ---: | --- | --- |
| `DEVICE_INFO` | `0x01` | page, `0`, `0` | page, value-low, value-high |
| `PING` | `0x02` | token bytes | identical token bytes |
| `ECHO` | `0x03` | test bytes | identical test bytes |
| `PATTERN_TEST` | `0x04` | A, B, C | `A xor A5`, `B + 3D`, rotate-left(C, 1) |
| `GET_STATS` | `0x05` | index, `0`, `0` | index, 16-bit value little-endian |
| `CLEAR_STATS` | `0x06` | `C3 3C 00` | `C3 3C 00` |

All arithmetic in `PATTERN_TEST` wraps at eight bits.

### 4.1 DEVICE_INFO pages

The PC reads all pages and accepts the target only after the protocol version,
identity syntax, device label, and required command capabilities are valid.
The workflow is not restricted to one MCU target ID or part number.

| Page | Bytes 5..6 | Meaning |
| ---: | --- | --- |
| 0 | `01 00` | Protocol version 1.0 |
| 1 | implementation-defined | Non-zero, non-`0xFFFF` logical target ID |
| 2 | ASCII | Device-label characters 0..1 |
| 3 | ASCII | Device-label characters 2..3 |
| 4 | ASCII | Device-label characters 4..5 |
| 5 | ASCII | Device-label characters 6..7 |
| 6 | `3F 00` | Supported-command capability bits |

The label is one through eight printable ASCII characters with zero padding.
The F13 reference reports target `0xF13B` and label `R5F10BMG`.
The F24 device reports target `0xF24B` and label prefix `R7F124FP`;
the eight-character label is the protocol limit, not a complete part number.
Capability bits 0 through 5 respectively advertise `DEVICE_INFO`, `PING`,
`ECHO`, `PATTERN_TEST`, `GET_STATS`, and `CLEAR_STATS`. The PC workflow requires
all six bits and rejects an incompatible version, reserved target ID, malformed
label, or incomplete capability set.

### 4.2 GET_STATS indexes

All counters saturate at `0xFFFF`.

| Index | Value |
| ---: | --- |
| 0 | Received protocol frames |
| 1 | Responses accepted by the CAN TX buffer |
| 2 | RX FIFO lost indications |
| 3 | TX failures/timeouts/aborts |
| 4 | Protocol errors |
| 5 | CRC errors |
| 6 | Byte-identical duplicate requests |
| 7 | CAN warning entries |
| 8 | CAN error-passive entries |
| 9 | CAN bus-off entries |
| 10 | Last raw CAN driver result |
| 11 | Last captured Classical channel-error flags (C0ERFL bits0..14) |

`CLEAR_STATS` clears the counters before its response is queued. Consequently,
the clear response itself makes TX count equal to one if it is accepted by the
controller.

Warning/passive/bus-off counters count captured hardware indications. Latched
flags can coalesce repeated events; these are not exact physical error totals.
F13 captures flags in its error ISR; F24 polls/clears the same Classical flag
positions from the main loop. The F24 driver callback's TDCV source mask is
not a raw C0ERFL value and is not passed to this decoder. Raw driver-result
numbers (index10) remain platform-specific.

## 5. Sequence, duplicate, and timeout behavior

- Sequence zero is reserved and rejected.
- Each new logical PC request uses the next non-zero sequence. CAN transport
  retries of that logical request reuse the original byte-identical frame and
  sequence.
- A byte-identical retry of the most recent request replays the cached response
  and increments the duplicate counter.
- Reusing the most recent sequence with different valid content returns
  `BAD_SEQUENCE`.
- F13 handles only one response transmission at a time. New requests remain in
  the four-entry hardware RX FIFO while TX is active.
- TX completion is polled in the main loop. After 100 ms, F13 requests an
  abort; an abort that does not complete within another 100 ms enters a
  recoverable TX-fault state without blocking the main loop.

## 6. Verification status

### 6.1 PC/target text-log comparison

Both device targets have a default-on
`CAN_TRANSFER_UART_TRACE_ENABLE` diagnostic. Main-loop protocol processing,
never an ISR, prints one `TRACE CT RX` line for the received request and one
`TRACE CT TX` line after the response is accepted by the CAN TX driver. Each
line includes ID, DLC, all available frame bytes, sequence, command, and the
independently calculated CRC result.

For a short integrity capture, enable `Detailed trace (slower)` and `Start Log`
in FirmwareUpgradeTool while separately saving the target UART0 terminal output.
Compare:

- PC `TRACE CAN TX id=0x710` to device `TRACE CT RX id=0x710`.
- Device `TRACE CT TX id=0x711` to PC `TRACE CAN RX id=0x711`.

ID, DLC, all eight data bytes, decoded sequence/command, and `crc=OK` must agree.
PC/target timestamps are independent and must not be compared. The target
trace uses blocking UART0 output at 115200 baud, so set
`CAN_TRANSFER_UART_TRACE_ENABLE` to `0` and rebuild before throughput,
queue-stress, or timing measurements.

At startup both targets report `CAN init: OK/FAIL`, the driver result,
channel status, the compiled Classical 500K setting, request/response IDs,
exact request filter, and P12 transceiver-standby output. F24 additionally
reports initialization stage and global/channel status. Only a successful
clock check enables its diagnostic UART; an absent startup log can therefore
require checking the clock before CAN. F13's channel-error ISR and F24's
main-loop polling capture flags; `CAN_TransferProtocol_Process()` prints one
coalesced `TRACE CT CAN_ERROR` line with raw flags and decoded controller error
names, limited to one line per 250 ms during repeated faults. This allows a
timeout with no `TRACE CT RX` to be separated into CAN
initialization, physical/bit-timing, and protocol stages without formatting
text inside an ISR.

Field/stage interpretation is maintained in the
[Custom Transfer diagnostic guide](https://released.github.io/slide_isp_tool_for_RL78/#transfer-diagnostics).

### 6.2 Verification references

Both device projects and the PC workflow clean-build. The PC target client drives all commands
through generic `CAN_TX_WAIT_RX` with response ID `0x711`, mask `0x7FF`, DLC 8,
and standard Classical data-frame matching. Unit tests cover CRC, request
encoding, response identity/ID/sequence/CRC checks, error decoding, complete
multi-page discovery including a non-F13 identity, every command workflow,
exact match construction, and advancing non-zero sequences.

F24 device discovery and a successful normal transfer test are illustrated by
paired Tool/MCU UART captures in the
[Custom Transfer operation example](https://released.github.io/slide_isp_tool_for_RL78/#f24-custom-transfer-example).
Hardware filter rejection, bus error injection, FIFO overflow, duplicate replay,
TX timeout recovery and analyzer waveforms need their own records; the normal
operation captures and software models do not establish those conditions.
