# Bridge self-update contract3 over Host Link v2 /128

Status: implemented software / F24 build and mock validation; physical gates open.
Canonical definitions: firmware/common/bridge_update_contract.h/.c.
This is PC -> HOST BRIDGE UART, never an external target profile or CAN command.

Feature bit10 (0x400) advertises descriptor/update support. A boot-only bridge
may report this feature with no CAN/I2C feature, zero CAN bytes and no bus ports.
Firmware without this feature is never queried/programmed with50..58.
Only128-byte hosts and bridge contract3 are accepted. Legacy96-byte hosts,
contract1 manifests/descriptors and single-manifest BEGIN are not supported.
Header CRC remains CCITT-FALSE over bytes2..9; payload CRC covers payload only.

| Command | Request | Successful response / state |
| --- | --- | --- |
| 50 INFO | empty |96-byte BUI1 descriptor |
| 51 ENTER | board LE16, contract3, B0 |empty ACK flushed before app reset to boot |
| 52 BEGIN |total LE16=256, full-region CRC32 LE32 |next offset LE32 (initial0); erase only after complete staged-region validation |
| 53 DATA |offset LE32 +4..64 bytes, aligned4 |next offset LE32 |
| 54 END |image CRC32 LE32 |same CRC32, commit marker stored; remain boot |
| 55 ABORT |empty |empty; idempotent, never erase |
| 56 RUN |board LE16, contract3, A0 |empty ACK flushed before jump; full flash CRC required |
| 57 READ_METADATA |offset LE16, count LE16=1..64 |offset LE16,total LE16=256,data[count]; installed Flash region, no erase |
| 58 STAGE_METADATA |offset LE16,total LE16=256,data[1..64] |next offset LE16,total LE16=256; RAM only, no erase |

App supports INFO/ENTER/READ_METADATA; boot supports all commands above.
F24 App1.4.1 fixes the outer Host Link dispatcher missing READ_METADATA(57)
despite the inner handler already supporting it. App1.4.0 can report valid INFO
then return BAD_COMMAND57 on a fresh probe. Do not bypass installed metadata
verification. Hold physical SW1/P32 low during reset/power-up to stay in the
unchanged Boot1.2.0, then use Misc115200 to install the fixed App and explicitly
RUN. This recovery updates App only; no boot reflash/programmer is required.
READ_METADATA is blocked during an active flash update. Reads/staging repeat
the same command with offsets0/64/128/192; no per-page command IDs.
New programming always ABORTs stale staging first, then stages all four pages.
Missing/future/conflicting/out-of-range pages reject without erasing. Exact
duplicates are idempotent. ABORT/reboot discard staging. An idle incomplete
stage cannot change Flash; a new attempt starts with ABORT. BEGIN must match
the whole-region CRC and identity/geometry before its first app erase.
Standard statuses03/04/05/09/0A/0B mean argument/CRC/sequence/incompatible/state/
internal flash failure. Sequence replay compares the complete original request,
not just a CRC. One identical in-flight retry reuses the cached result, not a
second side effect. Same sequence/different payload is rejected. PC attempts2,
5s each for update commands; ENTER/RUN one attempt. ABORT cleanup uses a fresh
sequence and bounded1s attempt even when the user cancellation flag is set.

All fields little-endian. Manifest location is image offset100h; image starts
at app_start and includes the LE32 tail CRC. Image4-byte aligned, >=minimum,
<=capacity. CRC32/ISO-HDLC: polyEDB88320 reflected, init/xorFFFFFFFF, covers
image[0..size-5]. No fixed60K/110K define is required. Byte holes areFF.

Metadata region is256 bytes at image offset100h..1FFh;128 defined core bytes
follow below, then128 reservedFF bytes at80h..FFh. The header's8-bit size128
means core length, never total region256; command offsets/total use LE16.
Minimum image is204h=516 bytes so the final CRC never overlaps metadata.
The full region is protected by image CRC and streamed DATA equality checks.

| Offset | BUR1 core manifest (128 bytes within256-byte region) |
| --- | --- |
|00 |magic BUR1=31525542h |
|04..07 |contract3, core size128, role2 HOST APP, checksum1 CRC32 |
|08/0A |board ID LE16 / contract ID LE16=2 |
|0C/10/14 |image size / version00MMmmpp / build (LE32 each) |
|18/1C/20/24 |app start / capacity / flash bytes / boot bytes |
|28/2C/30 |erase bytes / minimum image bytes / UART baud |
|34 |family ASCII12, zero-padded |
|40 |part ASCII16, zero-padded |
|50 |minimum compatible boot version |
|54/58/5C |reserved LE32 zero |
|60..7F |common UPD1 policy32: HOST_APP/UART, IRQ ABI, minimum boot; see COMMON_IMAGE_METADATA.md |

| Offset | BUI1 descriptor (96 bytes) |
| --- | --- |
|00 |magic BUI1=31495542h |
|04..07 |contract3, mode1 APP/2 BOOT, max DATA aligned4<=64, flags |
|08/0A |board ID / contract ID2 (LE16) |
|0C/10/14/18/1C |boot version/build, installed app version/build/size |
|20/24/28/2C |app start / capacity / flash / boot bytes |
|30/34/38/3C |erase / minimum image / UART baud / IRQ ABI |
|40/50 |part ASCII16 / family ASCII12 |
|5C |reserved0 |

Flags: bit0 installed app validated by flash CRC, bit1 active update,
bit2 flash backend ready. Missing app fields areFFFFFFFF. Fields are live
reports, not authenticated identities. PC checks board ID against GET_HOST_INFO,
fixed BOOT UART against a BOOT session (APP may negotiate a different Host
Link speed); BEFORE BEGIN checks exact family/part/geometry/
UART and minimum boot version. No arbitrary relocation or raw-file fallback.

Misc ENTER uses descriptor.uart_baud for reconnect after reset, never the
temporary APP Host Link speed. BUR1 UART remains the fixed bridge boot-update
contract. Future DEVICE UART policy version2/metadata112 is a different role
and is not accepted by this HOST parser; current HOST policy remains version1.

BEGIN invalidates old app by erasing its first block. Later blocks erased only
as sequential DATA reaches them. All256 metadata bytes must equal staging. Offset100h
magic is withheld; END verifies complete staged-image CRC and tail, writes
magic last, then validates actual flash again. On failure/ABORT app may be lost,
boot is retained. Boot/options/data/debug flash are never exposed to commands.
Boot validates full CRC before default auto-jump after reset and before RUN.
After an acknowledged END it waits indefinitely for explicit RUN or reset.

F24 geometry is in f24_bridge_layout.h; preparation derives IRQ projections
while each owned MTPJ retains its linker settings. Geometry changes must align
those settings with the layout and pass post-build bounds checks; preparation
does not silently replace project options. Packaging derives length and metadata.
Only Bridge_boot and Bridge_app are products; f24_board is shared source.
RAM flash-mode execution, startup stack,
IRQ dispatcher and device-file placeholder filtering are required parts of the
backend, not portable assumptions for F14/RA6. Future boards implement this
same descriptor/command contract with their own protected backend.

Physical supply, UART/flash timing, IRQ return, power failure, reset recovery
and analyzer evidence remain manual. See SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md.

## Stay-reason diagnostics and explicit RUN confirmation

59 DIAGNOSTICS takes an empty request and returns8 bytes: U8 version1, U8
hold reason, U8 reserved0, U8 current flags, LE32 accepted next offset.
Reasons0 valid/no hold,1 no App marker,2 malformed metadata,3 contract mismatch,
4 CRC mismatch,5 SW1 recovery,6 Flash init/erase/write failure,7 ENTER,
8 END complete awaiting explicit RUN. This does not change the96-byte descriptor.
App owns59 as read-only inspection; Boot owns all erase/write/RUN commands.

END remains in Boot. Explicit RUN is sent once, then Tool closes/reopens the
same COM at115200 within a finite retry/time window. Recheck board identity,
APP mode, CRC-valid installed descriptor and all256 metadata bytes, plus exact
version/build/size captured before RUN. Lost ACK can be resolved by this read-only
confirmation, never by automatically repeating RUN or a flash operation.
Cancel stops confirmation without undoing a completed RUN. See
UPDATE_VALIDATION_zh-TW.md for reports and manual test acceptance.
