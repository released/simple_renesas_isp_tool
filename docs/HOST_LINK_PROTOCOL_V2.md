# Host Link Protocol v2

Bridge self-update extension: optional feature bit10 / commands50..58 are
specified in [BRIDGE_SELF_UPDATE_PROTOCOL.md](./BRIDGE_SELF_UPDATE_PROTOCOL.md).
This extension changes the HOST app over UART, not an external CAN/I2C device.
Boot-only hosts may advertise bit10 without CAN/I2C capabilities.

Status: implementation baseline

This protocol connects the Windows PC tool to a generic MCU CAN/I2C bridge over a
dedicated binary UART link. The first host implementation is RL78/F24. Future
RA2L1 and RA6 hosts must expose the same protocol.

Target-specific bootloader, flash, image, and test commands are not implemented
by the bridge. They are encoded as CAN frames or complete I2C transactions by
a PC target protocol. The additive I2C/port contract is
[Multi-bus bridge design](./MULTIBUS_BRIDGE_DESIGN.md); existing CAN framing
and layouts remain compatible.

## 1. First-release scope

- One PC connection.
- One CAN channel.
- One target device on the configured request/response IDs.
- The number of outstanding requests is bounded and advertised by
  `GET_CAPABILITIES`; the F24 host supports one active plus two pending CAN
  transactions. The current PC/F13 profile deliberately uses one at a time.
- Classical CAN at 250/500/1000 kbit/s; default 500 kbit/s.
- CAN FD at 250/500/1000 kbit/s nominal and fixed 2 Mbit/s data rate.
- Standard and extended data frames.
- Exact or masked receive filters; accept-all filters are prohibited.
- Maximum Host Link payload:128 bytes; maximum complete frame142 bytes.
- This build requires the128-byte capability.96-byte hosts are incompatible;
  update the PC tool and bridge firmware together. Wire framing remains v2.
- All multi-byte integer fields are little-endian.

Outstanding requests apply only to generic CAN transactions. Configuration,
statistics, stop and reset commands return `BUSY` while a CAN transaction is
active. Both endpoints use bounded RX/TX buffers.

## 2. UART frame format

| Offset | Size | Field | Value or meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | SOF0 | `0x55` |
| 1 | 1 | SOF1 | `0xAA` |
| 2 | 1 | Version | `0x02` |
| 3 | 1 | Frame type | Request, response, or event |
| 4 | 2 | Sequence | Request/response correlation value |
| 6 | 1 | Command | Command or event identifier |
| 7 | 1 | Status | Zero in requests/events; response status otherwise |
| 8 | 2 | Payload length | `0..128` |
| 10 | 2 | Header CRC16 | CRC over bytes 2 through 9 |
| 12 | N | Payload | Command-specific payload |
| 12+N | 2 | Payload CRC16 | CRC over payload; empty payload also has a CRC |

CRC parameters:

- Algorithm: CRC-16/CCITT-FALSE
- Polynomial: `0x1021`
- Initial value: `0xFFFF`
- RefIn/RefOut: false
- XorOut: `0x0000`

### 2.1 Frame types

| Name | Value |
| --- | ---: |
| Request | `0x10` |
| Response | `0x20` |
| Event | `0x30` |

A response uses the request sequence and command. Events use sequence zero and
do not require an acknowledgement; event order is the UART frame order.

### 2.2 Stream parser rules

1. Search for `55 AA` without discarding a possible overlapping `55`.
2. Reject any version other than `0x02`.
3. Validate the header CRC before trusting payload length.
4. Reject payload length greater than128 immediately.
5. Wait for exactly `12 + payload_length + 2` bytes.
6. Validate payload CRC before dispatching the command.
7. On a malformed frame, resynchronize at the next possible SOF pair.
8. Never allocate memory based only on an unvalidated wire length.

If a valid header identifies a request but its payload CRC is invalid, the host
returns `CRC_ERROR` with the same sequence and command. Noise that does not
contain a valid header is counted but receives no response.

## 3. Status values

| Name | Value | Meaning |
| --- | ---: | --- |
| `OK` | `0x00` | Operation completed |
| `BUSY` | `0x01` | Host cannot accept the operation in the current state |
| `BAD_COMMAND` | `0x02` | Command is unsupported |
| `BAD_ARGUMENT` | `0x03` | Payload or field value is invalid |
| `CRC_ERROR` | `0x04` | Host Link payload CRC failed |
| `SEQUENCE_ERROR` | `0x05` | Sequence reuse or ordering rule failed |
| `TIMEOUT` | `0x06` | UART/CAN/I2C transaction timed out |
| `CAN_ERROR` | `0x07` | CAN controller or bus operation failed |
| `OVERFLOW` | `0x08` | A bounded queue or FIFO overflowed |
| `INCOMPATIBLE` | `0x09` | Protocol, mode, or capability is incompatible |
| `BAD_STATE` | `0x0A` | Command is not valid in the current host state |
| `INTERNAL_ERROR` | `0x0B` | Unexpected host failure |
| `I2C_NACK` | `0x0C` | Address/data not acknowledged |
| `I2C_ARBITRATION_LOST` | `0x0D` | I2C master ownership lost |
| `I2C_BUS_ERROR` | `0x0E` | I2C peripheral/clock/bus failure |

Error responses may contain command-specific diagnostic fields. The PC must not
treat unknown trailing diagnostic fields as a protocol error.

## 4. Command identifiers

| Command | Value | Allowed state |
| --- | ---: | --- |
| `PING` | `0x00` | Any |
| `GET_HOST_INFO` | `0x01` | Any |
| `GET_CAPABILITIES` | `0x02` | Any |
| `RESET_HOST` | `0x03` | No active CAN transaction |
| `GET_PORT_INFO` | `0x04` | No active transaction |
| `GET_LINK_BAUD` | `0x05` | APP link descriptor; feature bit11 required |
| `SET_LINK_BAUD` | `0x06` | CAN stopped, no I2C reservation/transaction |
| `CONFIRM_LINK_BAUD` | `0x07` | Pending new-rate handshake only |
| `SET_CAN_CONFIG` | `0x10` | CAN stopped |
| `SET_CAN_FILTER` | `0x11` | CAN stopped |
| `CAN_START` | `0x12` | CAN stopped/configured |
| `CAN_STOP` | `0x13` | Any; idempotent |
| `SELECT_CAN_PORT` | `0x14` | CAN stopped, no I2C reservation |
| `CAN_TX` | `0x20` | CAN running |
| `CAN_TX_WAIT_RX` | `0x21` | CAN running |
| `CAN_BURST_WAIT_RX` | `0x22` | CAN running; Classical version1 bit12 / FD version2 bit13 |
| `GET_STATS` | `0x30` | Any |
| `CLEAR_STATS` | `0x31` | No active transaction |
| `SET_I2C_CONFIG` | `0x40` | CAN stopped, no I2C reservation |
| `I2C_TRANSACTION` | `0x41` | CAN stopped; active transfer permits identical EXEC retry or fresh-sequence ABORT only |

Event identifiers:

| Event | Value | Payload |
| --- | ---: | --- |
| `CAN_RX_EVENT` | `0x80` | CAN frame record |
| `CAN_STATE_EVENT` | `0x81` | CAN state and raw controller status |
| `HOST_ERROR_EVENT` | `0x82` | Error source and diagnostic value |

## 5. Host state model

The host starts with CAN stopped.

```text
RESET
  -> LINK_READY/CAN_STOPPED
  -> SET_CAN_CONFIG + one or more SET_CAN_FILTER
  -> CAN_START
  -> CAN_RUNNING
  -> CAN_TX or CAN_TX_WAIT_RX
  -> CAN_STOP
  -> CAN_STOPPED
```

Rules:

- `GET_HOST_INFO`, `GET_CAPABILITIES`, and `GET_STATS` are non-destructive.
- Configuration and filter changes require CAN stopped.
- `CAN_START` requires a valid configuration and at least one enabled filter.
- No enabled filter may use a zero mask.
- `CAN_STOP` is safe to repeat.
- A bus-off transition produces an event and error count. Bounded automatic
  recovery is also reported.
- `RESET_HOST` transmits its response, flushes UART TX, and then resets.
  The PC sends this side-effecting command once, closes the old session, and
  performs a bounded reconnect and identity probe on the same COM port.

## 6. Host information and capabilities

### 6.1 `PING`

Request payload: zero to128 arbitrary bytes.

Response payload: the exact request payload.

### 6.2 `GET_HOST_INFO`

Request payload: empty.

Response payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | Host board ID |
| 2 | 1 | Firmware major |
| 3 | 1 | Firmware minor |
| 4 | 1 | Firmware patch |
| 5 | 1 | Minimum Host Link version |
| 6 | 1 | Maximum Host Link version |
| 7 | 1 | Reserved, zero |
| 8 | 4 | Firmware build number |
| 12 | 4 | Firmware feature flags |

Initial board IDs:

| Board | ID |
| --- | ---: |
| RL78/F24 host board | `0x0101` |
| RA2L1 host board | `0x0201` |
| RA6 host board | `0x0202` |

Board IDs identify the host, not the CAN target.

### 6.3 `GET_CAPABILITIES`

Request payload: empty.

Response payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Feature flags |
| 4 | 2 | Maximum Host Link payload;128 for this implementation |
| 6 | 1 | Maximum CAN data bytes; 64 for F24 |
| 7 | 1 | Maximum outstanding generic CAN requests |
| 8 | 1 | UART RX queue depth in frames |
| 9 | 1 | CAN RX queue depth in frames |
| 10 | 1 | Filter slots |
| 11 | 1 | Reserved, zero |
| 12 | 4 | Supported nominal bitrate mask |
| 16 | 4 | Supported data bitrate mask |
| 20 | 4 | CAN timestamp frequency in Hz; zero if unavailable |

Feature flags:

| Bit | Meaning |
| ---: | --- |
| 0 | Classical CAN |
| 1 | CAN FD |
| 2 | Standard ID |
| 3 | Extended ID |
| 4 | `CAN_TX_WAIT_RX` |
| 5 | Asynchronous RX event |
| 6 | Hardware timestamp |
| 7 | Automatic bus-off recovery |
| 8 | Generic I2C master staged transport |
| 9 | Bus port catalog and CAN port selection |
| 10 | Bridge UART self-update |
| 11 | APP Host Link baud INFO/SET/CONFIRM (independent of bus ports) |
| 12 | Generic Classical standard CAN_BURST_WAIT_RX |
| 13 | Generic FD+BRS standard CAN_BURST_WAIT_RX version2 |

I2C-only bridges may advertise max-CAN-data0; enabled CAN bridges retain
8/64-byte CAN limits. Port descriptors constrain selectable per-port rates and
transfer sizes. F24 reports one CAN0 (flags5) and one IICA0 (flags3),512-byte
I2C TX/RX limits and100kHz only. GET_STATS retains the UART/CAN counter prefix;
the App telemetry extension is defined in section10.2. No I2C counters are implied.

Nominal bitrate-mask bits:

| Bit | Rate |
| ---: | ---: |
| 0 | 125 kbit/s |
| 1 | 250 kbit/s |
| 2 | 500 kbit/s |
| 3 | 800 kbit/s |
| 4 | 1 Mbit/s |

Data bitrate-mask bits:

| Bit | Rate |
| ---: | ---: |
| 0 | 1 Mbit/s |
| 1 | 2 Mbit/s |
| 2 | 4 Mbit/s |
| 3 | 5 Mbit/s |

The current F24 firmware reports nominal bits 1, 2, 4 (mask 0x16) and data
bit 1 (mask 0x02). Older 500K-only firmware reports nominal mask 0x04.

## 7. CAN configuration and filters

### 7.1 CAN mode values

| Mode | Value |
| --- | ---: |
| Classical CAN | `0x00` |
| CAN FD | `0x01` |

### 7.2 Bitrate preset values

Preset values use the corresponding capability-mask bit number. Nominal preset
`2` means 500 kbit/s and data preset `1` means 2 Mbit/s.

### 7.3 `SET_CAN_CONFIG`

Request payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | CAN mode |
| 1 | 1 | Nominal bitrate preset |
| 2 | 1 | Data bitrate preset; `0xFF` for Classical CAN |
| 3 | 1 | Flags; zero in the first release |

On a clock-initialization failure, the F24 backend returns `CAN_ERROR` with an
optional eight-byte error diagnostic: `C1 stage MDIV CKC PLLCTL PLLSTS CMC OSTC`.
Stages are 1=not initialized, 2=X1 stabilization timeout, 3=main-clock selection
timeout, 4=PLL lock timeout, 5=PLL selection timeout, 6=unexpected startup
configuration, 7=HOCO fallback timeout. This is backend-specific error detail;
successful configuration responses and generic commands are unchanged.
For the current 20-MHz F24 build, BSP must select X1 mode on its first CMC
write (0x51 currently), runtime must not rewrite this write-once register, and
PLL input is /4 x16 with output 80 MHz. CMC=0 indicates a startup mode problem
even when the reported stage is X1 timeout; it does not prove a bad crystal.

The current RL78/F24 implementation accepts:

- Classical CAN: nominal preset 1 (250K), 2 (500K), or 4 (1M), data `0xFF`.
- CAN FD: nominal preset 1, 2, or 4, data preset 1 (2M).
- Preset 3 is 800K, not 1M, and is rejected by the current implementation.

Response payload echoes the accepted configuration.

SET_CAN_CONFIG travels over USB-UART, so no CAN-500K command handshake is
needed. Complete/cancel active transactions and CAN_STOP first if necessary.
The F24 writes timing only in global/channel reset mode with a valid CAN
clock, and reapplies it after generated configuration during CAN_START.
Success leaves CAN stopped and does not change the target's bitrate.

Classical request payloads (command 0x10, without UART frame/CRC shell):

| Rate | Payload bytes |
| --- | --- |
| 250K | `00 01 FF 00` |
| 500K | `00 02 FF 00` |
| 1M | `00 04 FF 00` |

FD payloads replace the first byte by 01 and the third byte by 01.
The PC filters its dropdown by the probed host capability mask and rejects
unsupported mode/rate combinations before any UART write. Current F13 images
and devices advertise only 500K; their metadata checks remain mandatory.

### 7.4 `SET_CAN_FILTER`

Request payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | Filter index |
| 1 | 1 | Enabled: 0 or 1 |
| 2 | 1 | IDE: 0 standard, 1 extended |
| 3 | 1 | Frame type: 0 data, 1 remote |
| 4 | 4 | CAN ID |
| 8 | 4 | CAN ID mask |

Validation:

- Standard ID and mask must not exceed `0x7FF`.
- Extended ID and mask must not exceed `0x1FFFFFFF`.
- An enabled filter must have a non-zero mask.
- Initial target profiles use data frames only.
- The filter is applied only by a later successful `CAN_START`.

Current CAN Upgrade v5 bootstrap/F13 metadata default:

- PC/host transmits requests on standard ID `0x700`.
- Host receives responses on standard ID `0x701`.
- Host response filter ID is `0x701`, mask is `0x7FF`.

## 8. Common CAN frame record

The CAN record length is `12 + data_length` bytes.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | Channel; zero in the first release |
| 1 | 1 | Frame flags |
| 2 | 1 | DLC encoding |
| 3 | 1 | Actual data length in bytes |
| 4 | 4 | CAN ID |
| 8 | 4 | Timestamp ticks; zero in TX requests |
| 12 | N | Data bytes, `N = data_length` |

Frame flags:

| Bit | Name | Meaning |
| ---: | --- | --- |
| 0 | IDE | Extended identifier |
| 1 | RTR | Remote frame |
| 2 | FDF | CAN FD frame |
| 3 | BRS | CAN FD bit-rate switching |
| 4 | ESI | Error-state indicator; RX information |
| 5..7 | Reserved | Must be zero |

Validation:

- Classical data length is `0..8`.
- CAN FD data length is one of `0..8, 12, 16, 20, 24, 32, 48, 64`.
- DLC and actual data length must agree.
- RTR is invalid with FDF, BRS, or non-zero data length.
- BRS is invalid without FDF.
- Standard and extended ID limits apply.

## 9. CAN data commands

### 9.1 `CAN_START` and `CAN_STOP`

Request payload: empty.

Successful response payload is a four-byte raw/normalized controller state.

### 9.2 `CAN_TX`

Request payload: one CAN frame record. Timestamp must be zero.

Successful response payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | TX completion timestamp ticks, or zero |
| 4 | 4 | Raw/normalized CAN controller result |

An implementation may append the following 32-byte TX diagnostic extension.
Clients must accept the original 8-byte response and the extended 40-byte
response.

| Offset | Size | Field |
| ---: | ---: | --- |
| 8 | 8 | Four input frame-header words |
| 16 | 8 | First four input data words |
| 24 | 2 | TX message RAM ID low word |
| 26 | 2 | TX message RAM ID/control high word |
| 28 | 1 | TX message RAM DLC register |
| 29 | 1 | TX message RAM FD-control register |
| 30 | 8 | First four TX message RAM data words |
| 38 | 2 | RAM-window selector value before diagnostic read-back |

The initial F24 implementation advances TX completion from the main loop and
uses a bounded 1000 ms completion timeout. It does not block in an ISR or in a
Host Link command handler.

### 9.3 `CAN_TX_WAIT_RX`

This is a generic transport transaction. It contains no target-specific command
or status interpretation.

Request payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | Response timeout in milliseconds; `1..30000` |
| 2 | 1 | Retry count after the first TX; `0..3` |
| 3 | 1 | Expected RX DLC; `0xFF` means any valid DLC |
| 4 | 4 | Expected RX CAN ID |
| 8 | 4 | RX CAN ID mask; must be non-zero |
| 12 | 1 | Expected frame-flag value |
| 13 | 1 | Frame-flag comparison mask |
| 14 | 2 | Reserved, zero |
| 16 | M | CAN TX frame record |

The maximum request is 92 bytes with a 64-byte CAN FD payload.

Successful response payload is the first received CAN frame record matching the
ID, ID mask, selected flags, and optional DLC. The PC target profile interprets
target command and status bytes before sending another target request.

On timeout the diagnostic payload contains TX-attempt count and the last raw CAN
result:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | TX attempt count, including the first attempt |
| 1 | 3 | Reserved, zero |
| 4 | 4 | Last raw/normalized TX result |

For the F24 CAN backend, TX results are0=TRANSMITTING,1=ABORT_OVER,
2=TX_END,3=END_WITH_ABORT,and-1=ARG_ERROR (LE32 FFFFFFFF).
For example, `02 00 00 00 02 00 00 00` means two attempts and a final TX_END.
It does not prove target-software processing or a matching response.
The PC preserves the exact host status and appends decoded detail only after
validating all8bytes,attempt count1..4 and the reserved bytes. Unknown TX values
remain labeled UNKNOWN. A host-returned CAN timeout is classified as a bus
transaction failure,distinct from a missing UART Host Link response.

Retries can repeat a CAN request, so the target profile must select a retry
count consistent with target idempotency.

### 9.4 `CAN_RX_EVENT`

Payload: one received CAN frame record.

Only frames accepted by an enabled filter can generate this event. While
`CAN_TX_WAIT_RX` is active, its first matching frame completes that request;
other accepted frames may be delivered as events if queue capacity permits.

### 9.5 `CAN_STATE_EVENT`

Emitted for warning, error-passive, bus-off entry, and bus-off recovery. The
payload is:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | Raw channel error source flags |
| 2 | 2 | Raw global error source flags |
| 4 | 4 | Raw controller state |
| 8 | 4 | Cumulative arbitration-loss count |

Channel source flags:

| Bit | Value | Meaning |
| ---: | ---: | --- |
| 0 | `0x0001` | Bus error |
| 1 | `0x0002` | Error warning |
| 2 | `0x0004` | Error passive |
| 3 | `0x0008` | Bus-off entry |
| 4 | `0x0010` | Bus-off recovery |
| 5 | `0x0020` | Overload |
| 6 | `0x0040` | Bus lock |
| 7 | `0x0080` | Arbitration lost |
| 8 | `0x0100` | Transceiver-delay-compensation violation |

Global source flags:

| Bit | Value | Meaning |
| ---: | ---: | --- |
| 0 | `0x0001` | DLC error |
| 1 | `0x0002` | Message lost summary |
| 2 | `0x0004` | TX history entry lost |
| 3 | `0x0008` | Payload overflow |
| 4 | `0x0010` | RX FIFO 0 message lost |
| 5 | `0x0020` | RX FIFO 1 message lost |
| 8 | `0x0100` | Common FIFO message lost |

The PC decoder retains the raw value, expands every known bit, and labels any
future unassigned bits as `UNKNOWN(0xNNNN)` rather than treating them as zero.

### 9.6 `HOST_ERROR_EVENT`

Uses the same 12-byte diagnostic payload as `CAN_STATE_EVENT`. It reports
arbitration loss, bus error, overload, bus lock, TDC violation, and global CAN
errors. If one hardware callback contains both state and non-state sources, the
host may emit both event types with the same snapshot.

## 10. Statistics

### CAN_BURST_WAIT_RX extension (Classical bit12 / FD bit13, command22)

Request header12 bytes: U8 version1, U8 frame count1..10, LE16 response
timeout1..30000ms, LE16 reply ID, LE16 nonzero reply mask, U8 reply DLC0..8,
U8 reserved0, LE16 gap0..10ms. Then count compact records
`[LE16 standard ID, U8 DLC0..8, data[DLC]]`. Classical standard data only.
Validate every record and exact end before any TX. Ten full records use122
payload bytes. Successful response is the existing matched CAN record.

Version2 requires bit13 and CAN FD configuration. The same12-byte header uses
version2 and byte9=0x0C (FDF+BRS). Every record and expected response is standard
FD+BRS. Each record retains LE16 ID,U8 wire DLC,decoded-length data. DLC0..8
or15 is accepted;15 means64 data bytes. Reply DLC remains0..8. All records,
IDs,flags,lengths and exact end are validated before the first TX.
BEGIN8 + DATA64 + COMMIT8 uses101 payload bytes and three FD frames.
The bridge does not interpret those target commands or Flash semantics.
Version1 remains byte-compatible. See [FD operation and engineering](./CAN_FD_UPDATE_zh-TW.md).

Each frame must physically complete before the next is started; the main loop
advances TX/gap/RX without blocking. Capture the reply once the final TX has
started, even before its TX-complete callback. Intermediate frames never wait
for application replies. Timeout applies per TX and to final RX; abort retains
the existing100ms bound/stop fallback. PC deadline includes every TX plus RX.
No automatic CAN-burst retries. Identical UART active/queued/completed requests
use existing duplicate suppression/replay; a new burst can partially transmit
before failure. Only the target profile may decide whether retry is safe.
Errors retain generic status/diagnostic semantics; no F13 fields in the bridge.
See [v5 block guide](./CAN_BLOCK_TRANSFER_V5_zh-TW.md).

### 10.1 `GET_STATS`

Response payload is fourteen `uint32` counters in this order:

1. UART RX bytes
2. UART TX bytes
3. UART header/payload CRC errors
4. UART invalid-length/version/type errors
5. UART RX queue overflows
6. CAN RX frames
7. CAN TX frames
8. CAN RX lost/queue overflow frames
9. CAN TX timeouts/failures
10. CAN warning entries
11. CAN error-passive entries
12. CAN bus-off entries
13. CAN recovery count
14. Host command errors

Counters saturate at `0xFFFFFFFF`; they do not wrap silently.

### 10.2 Host telemetry extension

The App GET_STATS response is84 bytes: the original fourteen counters at0..55
followed by seven LE32 values. Offset56 UART RX high-water(bytes),60 UART TX
high-water(bytes),64 CAN RX high-water(frames),68 pending CAN queue high-water,
72 max queue wait(ms),76 max complete CAN transaction(ms),80 max HostLink_Process
call(ms). Resolution is1ms;0 can mean below resolution. Peaks are since boot or
CLEAR_STATS, not per-update interval maxima. Transaction time includes generic
TX/RX waiting; it is not physical Flash time. Tick deltas handle60000ms wrap.
The fixed Boot returns56 counter bytes without telemetry. PC accepts exactly56
or84, marks absence, and never fabricates peaks. UART driver overflow saturates
at65535; other wire counters saturate atFFFFFFFF. A reset/saturation/missing
snapshot makes interval deltas unavailable. Do not poll while a bus transaction
is active or clear counters implicitly during an update.

### 10.3 `CLEAR_STATS`

Request and response payloads are empty. Clear does not change CAN
configuration, filters, controller state, or the duplicate-response cache.

## 11. Sequence, retry, and duplicate safety

- PC request sequence starts at a non-zero value after opening a host session.
- The PC does not reuse a sequence while a request is active.
- A byte-identical retry uses the same sequence, command, and payload.
- The host caches at least the most recently completed request key and encoded
  response.
- A matching `sequence + command + payload CRC` retry returns the cached
  response without repeating CAN TX or another side effect.
- If a byte-identical retry arrives while a CAN transaction is still active,
  the host does not restart the transaction or send an interim response; the
  original transaction produces the eventual response.
- A new `CAN_TX` or `CAN_TX_WAIT_RX` sequence received while another CAN
  transaction is active is queued up to the advertised outstanding limit.
  Queue overflow returns `OVERFLOW`; non-CAN commands return `BUSY`.
- A byte-identical retry of an active or queued transaction does not enqueue a
  duplicate. Reuse of an active or queued sequence with different content
  returns `SEQUENCE_ERROR`.
- Reuse of a cached sequence with a different command or payload returns
  `SEQUENCE_ERROR`.
- A new session performs `GET_HOST_INFO` and `GET_CAPABILITIES` before CAN
  configuration. `RESET_HOST` clears the replay cache.
- Sequence wrap is allowed only after no request is active.

## 12. Dedicated binary UART rule

The Host Link UART carries binary protocol bytes only.

- Host firmware must not send `printf`, banners, prompts, or debug text on this
  UART.
- Interrupt handlers must not call formatted output.
- Host diagnostics are counters, response fields, or events.
- Human-readable decoding and log formatting belong to the PC tool.

## 13. Target-profile responsibility

A PC target profile owns:

- Target discovery and exact device identity.
- Request/response CAN IDs and expected filters.
- Classical CAN versus CAN FD compatibility.
- Target command encoding and response decoding.
- Firmware image format, manifest, target, size, and CRC validation.
- Target timeouts, retry policy, sequence behavior, and update state machine.

The F24 host must not contain F13 command IDs, target flash addresses, manifest
layout, DATA sequence meaning, or success-status interpretation.

## 14. F13 initial profile mapping

F13 Boot uses:

- Classical CAN, 500 kbit/s after the coordinated target timing change.
- Request ID `0x700` and response ID `0x701`.
- Exact host RX filter `0x701 / 0x7FF`.
- `CAN_TX_WAIT_RX` for HELLO, ENTER, metadata, START, END, and ABORT.
- `CAN_BURST_WAIT_RX` for BEGIN/raw64-byte block segments/COMMIT, then one ACK.
- One target request at a time.

The current target boot protocol is version5 with mandatory pre-START metadata staging. Changing its physical
bitrate does not change its target protocol version.

Custom Transfer v1 uses request/response IDs `0x710` / `0x711`, an exact standard-data
response filter `0x711 / 0x7FF`, and a deterministic fixed-frame test protocol.
It does not retransmit arbitrary received frames unchanged.

## 15. Conformance checks

An implementation is not Host Link v2 conformant until it passes:

- Known CRC vectors and encode/decode round trips.
- Split-frame and concatenated-frame stream parsing.
- Noise and overlapping-SOF resynchronization.
- Length0/128,142-byte complete frames, every stream split and rejected129-byte
  payload cases; explicitly reject a96-byte host capability.
- Wrong version/type/CRC handling.
- Duplicate replay without a second CAN transmission.
- Same-sequence/different-payload rejection.
- CAN record DLC/data-length validation.
- Zero-mask filter rejection.
- Wrong-state command rejection.
- UART and CAN queue-overflow accounting.
- Timeout, bus-off, recovery, disconnect, and reset behavior.

## 16. Optional APP UART Host Link baud negotiation

Canonical constants: firmware/common/host_link_baud.h. F24 APP powers up at
115200 and advertises bit11; fixed bridge BOOT does not advertise this bit.
Never send these commands to a host without the feature. No target speed or
image bytes change. Current presets are115200/460800/921600, mask bits0/1/2.

05 empty request ->20 bytes: version1/type1 UART/reserved00/00, LE32 bootstrap
at4, nominal current at8, supported mask at12, calculated actual baud at16.
06 request LE32 nominal baud -> exact4-byte echo ACK at OLD speed. Reject
unsupported rate/length with03, active CAN/I2C with01. Only after the complete
ACK leaves the UART does the main loop apply the new divisor. TX queue failure
cannot arm a switch; exact replay can arm it after its ACK is finally queued.
07 request same LE32 baud -> exact echo at NEW speed and commits the link.
During a pending change only INFO/CONFIRM and exact cached retries are allowed.
No confirmation within2000ms, including a stalled TX, aborts queued bytes and
restores115200. Clear old-speed parser/replay/RX state on each rate transition.
Normal transitions wait for physical TX completion; timeout is failure recovery,
not a successful partially transmitted ACK. Handle the60000ms tick wrap.

PC SET/CONFIRM each use one attempt, not an old-speed retry. Probe identity
again after confirmation. Close/exit best-effort restores115200; a confirmed
high-rate session survives idle and may survive abrupt cable/process loss.
Open tries115200/460800/921600/115200 only on the explicitly selected COM,
validates Host Link identity before negotiation, and has finite recovery.
Reset returns to bootstrap then renegotiates. No scan-and-probe of all ports.
Misc ENTER reconnects at live BOOT descriptor.uart_baud, not APP session baud.
BOOT fixed rate is never silently changed by a CAN/I2C/UI selection.

F24 retains SC CKM0=20MHz. SDR AC00/2A00/1400 produce integer-calculated
114942/454545/909090 baud respectively. Hardware FTDI/clock/interrupt margin
requires measurements; nominal support is not a physical acceptance claim.
Future native USB CDC must describe its transport explicitly; it must not
pretend an arbitrary COM line-coding value measures USB throughput.
See HOST_LINK_BAUD_zh-TW.md for fixed DEVICE UART metadata and manual tests.

