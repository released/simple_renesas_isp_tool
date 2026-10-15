# Adding a CAN target or workflow

The F24 host is a generic Host Link/CAN transport. It must not gain target flash
addresses, command constants, manifests, or device-specific state machines.

## Add a target that implements CAN Upgrade v5

Do not add an MCU-name UI profile for each RL78/F14, F15, F24, F25, D1A, RA, or
other part. A compatible target uses the existing `CAN Upgrade` workflow and:

1. Emits format6 metadata at image offset100h..1FFh:128-byte core and128FF
   reserved bytes, including UPD1 role/transport/IRQ ABI/minimum-boot policy.
2. Stores CRC32/ISO-HDLC little-endian in the final four aligned BIN bytes.
3. Implements INFO00..1A, HELLO/ENTER, metadata RESET/stage/SEAL before START,
   BLOCK_BEGIN/raw indexed data/BLOCK_COMMIT, END and ABORT.
4. Reports the same target/MCU/layout/CAN values from its device INFO pages as
   the app BIN declares in its manifest.
5. Enforces its own flash start, capacity, erase size, image minimum, target,
   protocol/format, received-byte count, manifest and CRC before app handoff.
6. Uses64-byte blocks with24-bit aligned offsets, full8-byte raw segments,
   bitmap/CRC/metadata validation before writes and no-rewrite last-block
   replay. INFO1A declares the eight-ID data window without control/reply overlap.
   Require host22/bit12; generic bridge never owns target CRC/Flash semantics.
7. Supplies a linker/post-build check equivalent to the F13 artifact flow.
8. Adds real device images/layout cases to the generic PC mock/unit tests.

The existing UI will show the new values and reject image/device disagreement.
`App Start Override=Auto` derives the address from metadata; an explicit value
must match and never relocates a BIN.

The current F24 App advertises Classical250K/500K/1M and FD nominal250K/500K/1M
with2M data. F13 itself remains Classical500K, as declared by its metadata.
A target requiring another bitrate also needs a host-firmware preset and PC
selection for that rate, but still does not need an MCU-specific update profile.

## Add a different diagnostic or incompatible update protocol

A genuinely different wire protocol may need a separate workflow:

1. Define identity, protocol version, mode/bitrate, IDs/filter, frame format,
   timeout/retry and duplicate behavior in its own protocol document.
2. Add a codec/client under `src/core`; it may depend on `can_host_client`, but
   generic Host Link/CAN modules must not include it.
3. Validate mode, ID/mask width, frame flags, DLC, length, command, sequence,
   status, identity, and checksum before UART traffic.
4. Add worker-session operations; only the worker owns serial and protocol
   clients. The UI submits commands and receives immutable events.
5. Use an exact non-zero default receive filter. Reject unsupported modes and
   filter combinations.
6. Add fake-transport coverage for boundaries, correlation, retry, duplicate,
   corrupt/out-of-order responses, disconnect, cancellation and invalid-input
   zero traffic.
7. Update architecture, protocol, checklist, handoff and self-check documents.
8. Clean-build PC/firmware and retain physical end-to-end evidence before
   declaring the workflow supported.

## Host independence check

The separation is correct when the same target workflow runs through an F24,
RA2L1, RA6, or other backend implementing Host Link v2, without changing target
frames or image rules. A new host implements the generic commands and
capabilities in `HOST_LINK_PROTOCOL_V2.md`; it does not copy F13 logic.
