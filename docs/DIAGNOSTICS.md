# Diagnostics and Logging

## Slow F13 update with zero errors

Tool0079 user log164225 has19224bytes/4806 DATA round trips in about142s,
without UART retries/timeouts or CAN errors. Tool0080 removes PC SerialPort's
short-reply20ms completion delay; it does not change the4-byte CAN DATA or ACK
contract, firmware, baud defaults or FTDI driver. First compare the same BIN,
115200/500K and identical Detailed trace setting, then test trace-off and higher
Host Baud separately. See [latency and A/B procedure](HOST_LINK_BAUD_zh-TW.md).
New physical throughput and USB driver latency remain unmeasured.

## Selecting the correct COM adapter

The selector shows Windows driver names, for example
`COM6: USB Serial Port (COM6)` or `COM7: JLink CDC UART Port (COM7)`.
Only the COM number is saved in the INI or used to open/reconnect the serial
port. Refresh keeps the selected number when it is still present; ports are
ordered numerically. Missing Windows metadata falls back to a plain COM name.
The queued open/probe log includes the displayed adapter name.

The wider field and dropdown reserve space for names; connection buttons wrap
below it when the window cannot fit them with the build information. A driver
name identifies an adapter, not the connected bridge firmware. Use
`Open + Probe Host` to validate F24/RA/other Host Link identity. Enumeration
does not open ports or send probe traffic to unrelated equipment.

## F13 reference-board supply requirement

QB-R5F10BMG-TB schematic R20UT2272XJ0200 page 2 shows IC2 TJA1050T VCC
connected to EVDD; EVDD/VDD are normally connected by the board short pad.
The CAN transceiver requires 4.75-5.25 V, not 3.3 V. P12 controls S (silent
mode), not a standby input; LOW selects normal transmission. Startup's older
`STB=P12_LOW` text describes that pin level but does not measure supply voltage.
Do not connect an additional 5-V supply while a 3.3-V source remains connected;
verify USB-UART voltage compatibility before changing the board supply.
Source: [NXP TJA1050 datasheet](https://www.nxp.com/docs/en/data-sheet/TJA1050.pdf).

The user's 2026-10-14 isolation test failed at 3.3 V and passed at 5 V.
Tool log `can_upgrade_tool_20261014_201510.log` then confirms F24 build40 /
Tool0063 end-to-end Custom Transfer discovery and tests: 22 RX/22 TX, zero
lost/TX-failure/warning/passive/bus-off/recovery counters. This establishes
normal 500K transport, not Boot/App programming, CLEAR_STATS, stress, or fault
recovery. `TRACE CT TX` means the target driver accepted a send request, not
that the physical transmission completed.

## Standalone bridge-board CAN TX self test

After `Open + Probe Host`, select `Classical CAN` and the desired advertised
`250K`, `500K` (default), or `1M`, then click
`Start Bridge Board Self Test`. The worker sends this frame once per second without
waiting for a target response:

- Standard ID: `0x123`
- DLC: `8`
- Data: `11 22 33 44 55 66 77 XX`
- `XX`: resets to `00` at each Start and increments once per frame, wrapping
  from `FF` to `00`

Hover over the `?` beside the button for the analyzer requirement and expected
frame summary. The button changes to `Stop Bridge Board Self Test`. Stop performs `CAN_STOP` but keeps the
COM/Host Link session open. Disconnect and application shutdown also cancel the
test and stop CAN. A CAN analyzer must show `0x123`, DLC 8, the seven fixed
bytes, and the incrementing final byte;
`0x000`, DLC 0 indicates the fault remains between the F24 TX descriptor,
RS-CANFD peripheral, pin routing, or transceiver path. This diagnostic uses the
existing Host Link commands and does not require an F13 target.

The current F24 variant keeps HOCO startup but switches the confirmed 20-MHz
X1 /4 x16 to PLL80/MDIV1 before peripheral start. RS-CANFD lite requires MDIV=1 and
`fCLK=fMP/2`; HOCO cannot be its communication clock. The build-30 trace
`can_upgrade_tool_20261014_182125.log` has correct `input_head/input_data` but
invalid or zero RAM data. This is consistent with the old invalid clock
configuration; reflash before evaluating the next read-back.

With the optional TX diagnostic response, ID `0x123`, DLC 8 should read back
as IDL `0x0123`, IDH `0x0000`, DLCREG `0x80`, FDCTR `0x00`, and the first
four little-endian data words `0x2211 0x4433 0x6655 0xXX77`.
`window=0` is the saved/restored window value, not evidence that page 1 failed
to select. If the external clock cannot initialize, the Tool reports the
X1/main-clock/PLL stage and no self-test frame is transmitted.

Bitrate changes travel by binary UART SET_CAN_CONFIG, not CAN_TX. Detailed
trace shows Classical payloads `00 01 FF 00` (250K), `00 02 FF 00` (500K),
or `00 04 FF 00` (1M), followed by the matching ACK. Changing a dropdown leaves
CAN stopped; Self Test then starts it. Set the analyzer to that same bitrate.
The current F13 target still uses 500K. Selecting another bridge rate does not
change the target, and incompatible CAN Upgrade metadata blocks programming.

## Normal logging

Normal UI logging records timestamped workflow stages, validated host and
target identity, effective CAN configuration/filter, image manifest fields,
calculated CRC, progress, cleanup result, and the final error or success. Use
`Start Log` to write the same UTF-8 records to a timestamped `.log` file.

Normal mode intentionally does not emit one UI line per DATA frame. This keeps
the UI/event queue from becoming the firmware-update throughput bottleneck.

## Detailed trace

Enable `Detailed trace (slower)` only for short diagnostic captures or when a
per-frame trace is required. It adds:

- Host Link UART TX/RX/event/retry/timeout/unmatched records with sequence,
  command, status, attempt, and exact payload bytes.
- CAN TX/RX records with direction, ID, IDE/RTR/FDF/BRS flags, DLC, timestamp,
  and exact data bytes.

A maximum-size 120 KiB F13 update contains 30,720 DATA transactions; shorter
images contain `actual image size / 4` transactions. Detailed trace can
therefore slow an update and overwhelm the bounded UI event queue. For release
timing measurements leave it off and capture CAN with external hardware.

For the short `Custom Transfer` integrity test, the PC CAN trace additionally
decodes `CUSTOM_TRANSFER`, sequence, command, and CRC-8 result. The F13
reference target can simultaneously write `TRACE CT RX/TX` lines to its UART0
terminal. Save both logs and compare PC TX `0x710` to target RX `0x710`, then
target TX `0x711` to PC RX `0x711`. Compare ID, DLC, all eight bytes, sequence,
command, and CRC result; do not compare endpoint-local timestamps. Disable the
target's `CAN_TRANSFER_UART_TRACE_ENABLE` option for performance/stress tests.

The F13 reference target reports `CAN init: OK/FAIL`, the numeric driver result,
channel status, effective 500-kbit/s configuration, IDs, exact filter, and
transceiver standby state at startup. Channel error flags are captured in the
ISR and later printed from the main loop as `TRACE CT CAN_ERROR`, including
decoded bus, warning, passive, bus-off, overload, lock, arbitration, bit,
stuff, form, CRC, and ACK conditions. Repeated flags are coalesced and output is
limited to one line per 250 ms. No target ISR performs formatted output.

## Session summary

`Export Summary` creates a UTF-8 text report containing tool build, last
validated host/target identity, effective CAN configuration, selected filter,
image identity, Host Link request/retry/timeout/unmatched/event-drop counters,
the latest F24 `GET_STATS` snapshot, elapsed operation time, and final result.
The F24 snapshot contains UART RX/TX bytes, UART CRC/invalid/overflow errors,
CAN RX/TX/lost/failure counts, warning/error-passive/bus-off/recovery counts,
and Host Link command errors. It is refreshed after host probe, target
discovery/test, boot validation, and CAN stop. Export it before changing
profiles when investigating a failure.

If no successful `GET_STATS` response has been received, the summary explicitly
marks the F24 counters unavailable. Counter values prove that firmware observed
an event; they do not by themselves prove electrical timing or recovery on the
physical bus.

## Asynchronous host events

While the worker is idle it polls and drains bounded Host Link events. During a
synchronous request the same client routes interleaved events without confusing
them with the matching response.

- `CAN_RX_EVENT` reports an accepted CAN frame that did not complete the active
  wait-for-match transaction.
- `CAN_STATE_EVENT` reports warning, error-passive, bus-off, and recovery state.
- `HOST_ERROR_EVENT` reports arbitration loss, bus/overload/lock/TDC, and global
  controller error sources.

State/error payloads contain raw channel and global source bits, raw controller
state, and the cumulative arbitration-loss count. The PC logs the raw values
plus decoded source names such as `BUS_ERROR`, `ERROR_WARNING`, and
`MESSAGE_LOST`, and retains the dedicated arbitration-loss message. Event-queue
overflow is counted; use `GET_STATS` and an external CAN trace when events
arrive faster than the PC can display them.

## Host reset diagnosis

`Reset Host` stops CAN, sends `RESET_HOST` once, waits for its ACK, closes the
old COM session, and retries the same port/baud for up to about three seconds.
Each successful reopen must pass F24 identity and capability validation before
target controls are re-enabled. A missing ACK is logged as a warning because
the subsequent identity probe determines whether the reset actually completed.

P15/P16 carry binary Host Link frames only. The F24 firmware does not link
`printf`/`putchar`; add human-readable diagnostics to PC decoding instead of
writing text to the MCU UART.

The summary does not replace the raw `.log`, external CAN trace, CS+ build log,
or physical test record. Archive all four for release verification.

## Typical diagnosis order

1. Confirm `Open + Probe Host` reports board `0x0101` and Host Link v2.
2. Confirm the selected workflow, Classical/CAN FD mode, bitrate, and effective
   non-zero filter.
3. Use normal logging for one reproduction.
4. If the stage is still ambiguous, enable detailed trace for the smallest
   reproducible operation.
5. Compare UART sequence/retry/timeout records with CAN TX/RX direction and
   target status.
6. Export the session summary and preserve the exact tool/F24/F13 versions.

For a `CAN_TX_WAIT_RX` timeout where F24 TX increases but RX stays zero:

- `CAN init: FAIL` on the target is an initialization fault.
- `CAN init: OK` with no target `TRACE CT RX` means no valid request reached
  the target controller. Check CANH/CANL polarity, common ground, both
  transceiver standby pins, termination, power, and actual bit rate.
- A target `TRACE CT CAN_ERROR` line narrows the failure to its decoded
  controller condition.
- Target RX without TX points to target protocol/TX processing; target RX and
  TX without F24 RX points to the response path or host RX/filter setup.
