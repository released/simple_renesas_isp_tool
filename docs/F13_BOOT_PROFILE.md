# CAN Upgrade v5 target contract and RL78/F13 baseline

Status: software implementation and mock verification are complete. Physical
PC/F24/F13 verification is not yet run.

Current image format6 reserves256 metadata bytes at app offset100h..1FFh:
128-byte defined core,128 reservedFF. Code still starts at offset200h. Format3
and format4 callback-IRQ images are rejected. CAN protocol is5; paired boot/app are1.5.0. PC and
bridge additionally require Host Link payload128; reflash matching firmware.

`FirmwareUpgradeTool` does not select a flash layout from an MCU-name profile. The
`CAN Upgrade` workflow reads the layout and identity from the application BIN,
then reads the running device's copy through the non-destructive INFO command.
Programming is enabled only when both sources agree. The F24 transports CAN
frames and does not interpret target commands, manifests, or flash addresses.

`Custom Transfer` is a separate diagnostic workflow; it is not a firmware layout
profile.

Format5 also fixes the normal interrupt ABI: boot owns04..7C positions and
tail-dispatches owner55 to app2000+vector*2; app handlers always return RETI.
No ordinary shared timer callback remains. App-local handler/identity generation
supports different compatible app projects without rebuilding boot. Physical
BRK7E/reset are not ordinary forwarded IRQs. Repartitioning, changing MCU/RAM/
startup or boot-used peripherals is a different contract and may require boot.

## What the UI can know and when

Immediately after an application BIN is loaded, without opening a COM port,
the tool displays:

- exact BIN size in decimal and hexadecimal;
- calculated CRC32, stored final-four-byte CRC32, and match/mismatch result;
- MCU family and part, target ID, app semantic version and build;
- image format, update protocol ID/version and checksum type;
- application start, actual image size, application capacity, total code flash,
  boot-reserved size, erase-block size, and minimum supported image size;
- supported CAN mode, nominal/data bitrate masks, request/response IDs, and
  standard/extended ID format.

After `Open + Probe Host` and `Connect Target` (`Discover Target`), INFO pages
add the live device data:

- running state;
- boot semantic version/build;
- installed app semantic version/build and actual installed image size;
- device MCU/layout/CAN fields matching the list above.

The tool does not load or program a bootloader BIN, so live boot version and
device configuration cannot be inferred from the application BIN. Those
values intentionally require the read-only handshake. Supporting offline
inspection of a bootloader artifact would require a separately defined boot
artifact manifest and is outside the current update flow.

## Metadata contract, not an MCU profile

The raw update BIN starts at its declared application address. Its 128-byte
format-6 core is at image offset `0x100` within a256-byte metadata region; CRC32/ISO-HDLC occupies the final
four bytes. The manifest contains the identity, layout and CAN fields listed
above. The exact field table is maintained in the paired target protocol:

- [F13 CAN Boot protocol](https://github.com/released/RL78_F13_Boot_loader_CAN/blob/main/CAN_BOOT_PROTOCOL.md)

An image with only a trailing checksum cannot safely tell the tool its MCU,
application address, flash bounds, protocol, or CAN settings. Therefore every
target that uses this generic flow must implement both the format-6 metadata region
and the CAN Upgrade v5 INFO pages. A future RL78/F14, F15, F24, F25, D1A, or RA
target can change the field values without adding another MCU-specific UI
profile.

The optional `App Start Override` is stored in the INI. `Auto` uses manifest
metadata. A numeric override is a safety assertion, not a relocation command:
it must exactly equal the BIN and discovered-device start address or the tool
blocks programming.

Every UI metadata value has its own labeled field. Before a BIN is loaded the
value is `N/A`; the tool never fills missing manifest values from an MCU-name
guess. For an invalid BIN, independently calculable size/checksum values may
remain visible, while manifest-dependent fields remain unavailable and the
metadata status reports invalid.

## Current RL78/F13 values

These are values emitted by the current R5F10BMG projects, not PC-tool limits.

| Field | Current value |
| --- | --- |
| MCU family / part | `RL78/F13` / `R5F10BMG` |
| CAN | Classical CAN, 500 kbit/s |
| Request / response ID | Standard data `0x700` / `0x701` |
| Protocol / image format | CAN Upgrade `1.5` / format `6` |
| Logical target | `0xF13B` |
| Code flash | `0x20000` bytes |
| Boot reserved / app start | `0x2000` / `0x2000` |
| Application capacity | `0x1E000` bytes |
| Erase block | `0x400` bytes |
| Update image size | Variable, 4-byte aligned, metadata-defined |
| Integrity | CRC32/ISO-HDLC in the final four image bytes |

The post-build app used in the latest software check is only about 17 KiB.
The automated suite also covers aligned 32/60/80/120-KiB images, an alternate
start/layout, and an image larger than 256 KiB. Capacity is a maximum writable
region, not a required padded BIN length.

## Commands and address range

All multi-byte fields are little-endian. The F13 implementation supports
HELLO, metadata staging/seal, START, BLOCK_BEGIN/COMMIT, END, ABORT, ENTER and
paged INFO. A64-byte block uses a24-bit offset and CRC32:

```text
[0x0B, offset0, offset1, offset2, crc0, crc1, crc2, crc3]
ID720..727: up to8 raw image bytes each
[0x0C, offset0, offset1, offset2, crc0, crc1, crc2, crc3]
```

This removes the old 16-bit-word-sequence limit. The wire format can address
up to `0x01000000` bytes per image; each target still enforces its own declared
capacity and flash boundary.

## Programming prerequisites

`Start Firmware Update` stays disabled until all of these conditions hold:

1. The host is probed, Host Link v2 is compatible and feature bit12 is present.
2. `CAN Upgrade` is selected and no worker operation is active.
3. The loaded BIN has valid format-6 metadata and matching stored/calculated
   CRC32.
4. The selected CAN mode/bitrate and filter agree with the image metadata.
5. INFO/HELLO identify a compatible target currently in boot state.
6. Image and device target, protocol, format, MCU strings, flash layout,
   capacity, minimum size, CAN support, IDs, and ID format agree.
7. `App Start Override` is `Auto` or exactly matches both metadata sources.

The click handler repeats local validation before START. The target then
independently validates START, every block offset/bitmap/CRC, manifest, FSL results,
received length, and final CRC before it can enter the application.

## User workflow

1. Load the app update BIN. Review checksum, identity, layout and CAN metadata
   before any bus operation.
2. Open and probe the F24 host.
3. Select `CAN Upgrade`, the metadata-supported CAN settings, and the exact
   filter. With current F13 defaults this is Classical 500K and `0x701/0x7FF`.
4. Choose `Connect Target`. Discovery reads INFO without resetting the target.
5. If the application is running, choose `Enter Bootloader`, then rediscover.
6. Confirm the image and device metadata shown by the UI are identical and the
   prerequisite line reports `Ready`.
7. Choose `Start Firmware Update` and confirm the summarized range/version.
8. The worker stages/seals metadata, sends START, monotonic64-byte blocks and END. Only matching ACKs
   advance progress; END must return the complete image CRC32.

Loading the BIN first is recommended for a future device that changes the CAN
IDs, because its manifest supplies the discovery IDs. Without a BIN, discovery
uses the current v5 bootstrap defaults `0x700/0x701` at Classical 500K.

## Recovery and security boundary

- Target response waits, erase-boundary waits, END verification, retry count,
  ABORT, and CAN stop are bounded.
- The F13 bootloader enables a30-second inactivity timer only when entering
  boot with an already valid app. Matching standard non-RTR ID700/DLC>0 frames
  refresh it before command validation. Before accepted START, expiry returns
  the old valid app; after accepted START it retains boot. Invalid-app recovery
  never enables this timer, even after START. ABORT/fatal errors disable later
  automatic return. See the full gate tables/diagrams in
  [F13 guide section2.2](./F13_BOOT_UPDATE_FLOW_zh-TW.md).
- The last committed block is replay-safe without erase/write; other stale offsets are rejected.
- A partial image, FSL failure, wrong manifest, or bad CRC remains in boot mode.
- F24 and F13 deliberately do not use WDT.
- CRC32 provides corruption/interruption detection, not authenticity. Digital
  signatures, authentication, and anti-rollback remain out of scope.

Full update, power interruption, shared-ISR runtime, FSL fault, bus-off, filter,
timing, and recovery tests on physical hardware remain required before release.
