# Generic MCU Host Backend Guide

This guide defines how a future RA2L1, RA6, or other MCU host can replace the
RL78/F24 board without changing the PC target profiles or F13 firmware. The
wire contract is `HOST_LINK_PROTOCOL_V2.md`; this document describes the port
boundary and acceptance gates.

## Compatibility boundary

The host is a transport bridge. It owns UART framing, CAN configuration,
filters, queues, controller state, retries requested by Host Link, and host
statistics. It must not contain F13 command values, flash addresses, image
formats, update states, or target success rules.

The PC continues to own target discovery and target-profile logic. Therefore a
backend is compatible only when the same PC executable and F13 profile run
unchanged over the new board.

## Required services

Implement these layers behind the Host Link v2 command handlers:

1. A dedicated binary UART transport with bounded RX/TX queues. It must not
   emit banners, `printf` text, or ISR diagnostics on that UART.
2. A streaming Host Link parser with header/payload CRC validation, non-zero
   request sequences, bounded payloads, and byte-identical replay of the last
   completed request.
3. A CAN backend that can set a validated mode/bitrate preset, install an exact
   standard or extended data-frame filter, start/stop the controller, transmit
   one frame, and pop received frames from a bounded ISR-to-main queue.
4. A millisecond time source for bounded UART/CAN transactions and retry
   deadlines.
5. Saturating diagnostic counters and asynchronous event queues. ISRs only
   capture data, status, flags, and counters; parsing and formatting run in the
   main loop or task context.

The first compatible implementation must support Classical CAN at 500 kbit/s.
If it advertises CAN FD, it must also support the defined 500 kbit/s nominal / 2
Mbit/s data preset. A backend must reject unsupported presets instead of
silently substituting timing.

## Host Link conformance

Implement and test the commands and payloads defined in
`HOST_LINK_PROTOCOL_V2.md`, including:

- `PING`, `GET_HOST_INFO`, `GET_CAPABILITIES`, `GET_STATS`, and `CLEAR_STATS`.
- `SET_CAN_CONFIG`, `SET_CAN_FILTER`, `CAN_START`, and `CAN_STOP`.
- `CAN_TX` and `CAN_TX_WAIT_RX`, including exact ID/mask/flag/DLC matching,
  timeout, retry, duplicate-request replay, and explicit error status.
- CAN Upgrade v5 additionally requires feature bit12 / CAN_BURST_WAIT_RX22:
  fully validate up to10 compact Classical standard frames in128bytes before
  any TX; wait for physical completion per frame, then match one final reply
  (including a fast reply before final TX completion). Use bounded nonblocking
  gap/TX/RX/abort and ordinary UART duplicate replay. Never copy F13 CRC/Flash
  logic into this backend. See CAN_BLOCK_TRANSFER_V5_zh-TW.md and production
  can_burst codec / host_link_can_process.inc HAL tests.
- `CAN_RX_EVENT`, controller-state events, and host-error events for advertised
  capabilities.

Allocate a new host board ID; do not reuse the F24 value `0x0101`. Report the
actual queue sizes, modes, bitrate presets, filter count, maximum payload, and
optional features in `GET_CAPABILITIES`. The PC must be able to reject a board
that lacks the selected workflow's required capability before CAN traffic starts.

`GET_STATS` returns the same fourteen little-endian `uint32` counters and order
used by F24. A platform may collect additional private counters, but it must not
change the v2 payload. Extend the protocol version before changing its wire
layout.

## Porting sequence

1. Bring up the dedicated UART and prove arbitrary binary round trips without
   text output or dropped bytes.
2. Port CRC, framing, parser, response encoder, sequence/replay cache, bounded
   queues, and statistics. Verify malformed frames cause no CAN side effects.
3. Implement CAN stopped-state configuration and exact non-zero filters.
   Accept-all filtering is not supported.
4. Implement bounded start/stop/transmit/RX operations and normalize driver
   results to Host Link status values.
5. Add warning, error-passive, bus-off, recovery, RX-overflow, and TX-failure
   capture outside formatted-output paths.
6. Fill in identity/capability responses and connect the existing PC host probe.
7. Run protocol and fault-injection conformance before enabling an F13 update.

## Verification gates

A backend is not release-ready until all of the following have evidence:

- Host probe validates board ID, protocol version, firmware version, build,
  capabilities, and all fourteen statistics counters.
- Classical 500K and, when advertised, CAN FD 500K/2M pass with measured bus
  timing.
- Exact standard/extended filters accept intended frames and reject adjacent
  IDs, RTR, and incompatible frame formats.
- Duplicate requests do not repeat CAN side effects; changed-content sequence
  reuse is rejected.
- RX/TX queue boundaries, disconnect, timeout, retry, controller warning,
  error-passive, bus-off, and recovery behavior are captured in logs.
- The unchanged PC F13 Transfer tests pass, followed by the unchanged F13 Boot
  update workflow including cancel, inactivity, power interruption, and reset.
- Tool log, session summary, external CAN trace, firmware version, and build log
  are archived together.

Source review or a clean build cannot replace these physical checks. Do not
advertise a capability until its driver path and fault behavior have both been
verified on the selected board.
