# Firmware Upgrade Tool

Canonical workspace directory: `simple_renesas_isp_tool`. The product,
solution/project, executable and settings remain `FirmwareUpgradeTool` /
`FirmwareUpgradeTool.ini`; only the containing directory name changes.

Native Windows tool and MCU-bridge firmware for CAN/CAN FD transfer testing,
target firmware updates and staged I2C master communication.

The separate [F24 MCU CAN device design](docs/F24_CAN_DEVICE_DESIGN_zh-TW.md)
uses an8KiB Boot and App at0x2000, independent of the F24 host's Misc/UART
self-update firmware. Its Transfer/Boot/App builds and safe software models
are checked; F24 device hardware acceptance is still required.

## Downloads and operating guides

For editing device image metadata or adding App interrupts, use the
[F13/F24 CAN device workshop](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md):
editable versus generated fields, before/after examples, App-only commands
and BIN/vector verification. Both devices use app_image.json and
app_irq_handlers.json; generators own INC/header/vector outputs. F24 host
firmware is outside that tutorial.

Start with the [public download and workflow index](https://released.github.io/slide_isp_tool_for_RL78/#downloads):
Tool EXE, F24 Host Boot/App, F13 Device Boot/App, six operating/flowchart entries
and the [Boot/App porting guide](https://released.github.io/slide_boot_loader_for_RL78/#boot-app-porting).
The matching offline source is [the multi-bus guide](docs/MULTIBUS_TOOL_GUIDE_zh-TW.md#downloads).
Initial Boot installation uses a programmer; Misc updates the Host App only,
while CAN Upgrade updates the external Device App. These images are not interchangeable.

For the complete Tool-to-device teaching path, start with the
[CAN update walkthrough](docs/CAN_UPDATE_WALKTHROUGH_zh-TW.md) and its
[web tables/flows](https://released.github.io/slide_isp_tool_for_RL78/#can-update-walkthrough).
It maps UI actions, offline/live validation, F24 transport and F13 receive/Flash
functions, separating RAM staging, programming and post-reset App evidence.

For per-byte Flash addresses, metadata producer/parser examples and CAN payload
layouts, use the [Flash/metadata/CAN porting reference](docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md)
or its [web section](https://released.github.io/slide_boot_loader_for_RL78/#flash-metadata-can-reference).

Download this complete repository for F24 firmware; its Boot/App projects share
board, driver, SC, library and packaging sources. F13 integration tests use the
separate [Boot](https://github.com/released/RL78_F13_Boot_loader_CAN) and
[App](https://github.com/released/RL78_F13_Boot_loader_CAN_app) repositories.
Place them beside this checkout using their original names (remove `-main`
after ZIP extraction). Normal solution/one-click builds compile the PC Tool
only; explicitly build `tests/HostLinkProtocolTests.vcxproj` for integration tests.
For non-sibling checkouts, configure machine-local `FIRMWARE_F13_BOOT_PROJECT`,
`FIRMWARE_F13_APP_PROJECT` and `FIRMWARE_WEB_ROOT`; the C++ test project also
accepts `/p:F13BootProject=<checkout>`. No personal workspace layout is required.
After relocating/regenerating F24 SC output, use the documented projection
preparation in the [shared-console guide](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md).

The first supported path is:

```text
Windows PC Tool -> UART/USB VCOM -> F24 (MCU CAN host) -> CAN -> F13 (MCU CAN device)
```

Future host implementations are planned for RL78/F14, RA2L1 and RA6 while keeping the
same Host Link protocol and PC target-profile interfaces.
F24 (MCU CAN device) will be a separate Boot/App project, not the existing F24
host bridge images. Board role and metadata, not the MCU model alone, select
the update workflow; the host's own App is updated through Misc/UART.

## Current status

First engineering delivery (2026-10-15): keep Tool0082, F24 Boot1.2.0/App1.5.0
and F13 Boot/App1.5.0 paired. User log182253 confirms COM6/Host115200/CAN500K,
19244B,301 sequential block ACKs,10963ms/1755B/s and matching END CRC CA0DF55C,
with no reported retries/queue drops/CAN errors. This is one normal-path run,
not complete fault-matrix or post-reset App-launch acceptance. Upload files,
SHA-256, installation order and limits: [first delivery](docs/FIRST_DELIVERY_20261015_zh-TW.md).

0082 replaces per4-byte CAN DATA round trips with64-byte CRC-checked blocks,
using8-byte indexed raw CAN frames and one final block ACK. Generic bridge
CAN_BURST_WAIT_RX22/bit12 requires F24 App1.5.0; F24 Boot1.2.0 is unchanged.
Install the F13 Boot/App1.5.0 pair once; image format6,8-KiB Boot, App2000 and
fixed IRQ ABI remain unchanged. UI/worker reject missing burst capabilities.
19224bytes need301 block transactions instead of4806; this is not a measured
16x speed claim. See [v5 block protocol and installation](docs/CAN_BLOCK_TRANSFER_V5_zh-TW.md).
Real-source/model fault tests and artifact guards pass; the single v5 speed
run above is measured, while repeated/high-baud performance, physical
disconnect/power-loss/IRQ and native GUI acceptance remain pending.

0080 removes the PC serial receiver's fixed short-frame read delay: buffered
bytes return immediately; otherwise wait for first data within the caller's
deadline, at most50ms per idle wait. Fragmented frames still use the same
Host Link parser/CRC and retry/cancel rules. No F13/F24 firmware, wire contract,
baud default or FTDI driver setting changed. See [speed and latency checks](docs/HOST_LINK_BAUD_zh-TW.md).
The user supplied one successful F13 CAN update/reset/App-launch capture at
115200/500K with0079:19224bytes,4806 DATA exchanges,about142s DATA time.
User0080 log171150 shows about77s DATA/79s overall for that same image/CRC.
It has missing trace lines;0082 now warns on UI queue drops. Repeated v5 A/B
and physical high-baud reliability still require additional captures;
source/Win32-model tests are not hardware throughput measurements.

0079 prevents startup field notifications from saving before the INI path is
initialized. F24 App1.4.1 fixes missing Host Link READ_METADATA dispatch(57):
fresh probe still verifies all256metadata bytes. For an installed1.4.0 App
returning BAD_COMMAND57, hold SW1 during board reset to enter the unchanged
Boot1.2.0, then update the fixed App via Misc115200. Do not bypass metadata.

0078 fixes clipped update progress captions: a measured single-line operation
label reserves100%, with non-overlapping bar/elapsed/build fields inside the
footer. Only operation-name changes re-layout the console. GDI font/geometry
and PC regression tests cover the fix; actual desktop/DPI screenshots remain
pending. No firmware, wire protocol or update-safety behavior changed.

Folder layout: `docs` contains current user/developer guides and specifications;
`build` contains the current delivery EXE/INI and user runtime logs if recorded.
Build intermediates, test binaries, build logs, old tool backups and historical
verification/plan records belong under disposable `tmp/internal`. See the
[documentation index](docs/README.md). Deleting `tmp` loses those diagnostics
and old EXE/INI backups, not the current delivery/settings or firmware sources;
future builds recreate their temporary output directories.

Firmware developers: start with the [RL78 Boot/App porting guide (zh-TW)](docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md).
An [offline PDF](docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.pdf) is also provided.
It separates reusing a fixed boot contract from designing your own update flow,
with F13/F24 startup/linker/IRQ, app-local generation and acceptance checklists.
The earlier porting-documentation step kept Tool0077 and firmware unchanged;
0080 was the PC-only serial receiver fix;0082 is the coordinated v5 change above.

0077 adds APP Host Link115200/460800/921600 negotiation and bounded recovery.
The upper Host Baud is PC-to-bridge only, independent of CAN/I2C/device UART.
Fixed bridge BOOT self-update still uses115200; current split app is1.5.0.
Existing CAN/BUR1 images and the F13 boot/app wire contract are unchanged.
Future DEVICE UART policy version2 records compile-time device baud, with
parser/UI/gate and build helper prepared; its UART tab/backend/programming
workflow is NOT enabled. See docs/HOST_LINK_BAUD_zh-TW.md. Physical high-speed
FTDI/MCU/Flash verification remains open; no raw baud value proves throughput.

F24 now also supports independent app-local IRQ/identity generation. Each
compatible app folder owns app_irq_handlers.json/app_image.json; pre-build
generates its fixed table and identity header, and app-only build/package
accept -AppProject without regenerating boot. Actual A/B CC-RL fixture builds
use different IRQ names/maps and versions while boot hashes remain unchanged.
The IRQ engineering workflow retains the UART boot/BUR1 contract3 boundary;
PC0077 introduced APP link baud negotiation, retained in0078. Real-board app
swapping/IRQ/update acceptance remains pending.

F13 now has a fixed complete normal-vector interface: normal app-only IRQ
additions and app version changes require only an app update, not boot
reinstallation. The app-local handler map generates all fixed branch slots;
all normal ISRs, including shared INTTRJ0, return RETI. Reusing boot with a
different app project still requires the same MCU/partition/RAM/startup and
metadata/CRC/INFO-ENTER contract. Initial format6/v5 boot1.5.0/app installation
is required; old callback-format4 images are rejected. See the target IRQ
guides and the canonical F13 flow guide for integration and manual gates.

Host Link payload limit is128 bytes (complete frame142);96-byte hosts are no
longer compatible. Metadata owns app offset100h..1FFh (256 bytes), consisting
of128 defined core bytes and128 reservedFF bytes. F13 image format6 / paired
boot-app1.5.0 and F24 bridge contract3 require matching firmware. The v5 update
changes F24 App only, not its Boot.

Earlier0076 added shared UPD1 image role/update-transport/IRQ-ABI/minimum-boot
policy in the128-byte core of the unchanged256-byte metadata region. The
shared UI shows Update Policy and Boot / IRQ ABI; switching TAB never turns a
CAN BIN into a G16 raw image. UI/worker and boot reject incompatible paths.
F13 requires complete metadata staging/seal before START. F24 self-update
uses contract3 and validates the same policy before BEGIN. Host Link stays128.
G16 replacement boot/app development is explicitly deferred; programming stays
blocked. See docs/COMMON_IMAGE_METADATA.md for the exact contract and limits.
F24 stages metadata in four64-byte commands and validates it before BEGIN
erases; reads reassemble the same region. Minimum image size is516 bytes.

Functional changes synchronize the affected Markdown/PDF guides and local web
operation drafts in the same delivery. Screenshot and physical verification
gates remain explicit; local editing does not publish the website.

The build entry point normally closes only this project's running tool under
its delivery `build` or generated `tmp/internal/build` directory, allowing
settings/log/worker/COM cleanup before linking.
If normal close fails or exceeds15seconds, it stops rather than force-killing
an update. Other copies are untouched; CloseRunningTool=false opts out.

The single-screen shared console targets1366x768 without parent scrolling:
interface settings left, one shared BIN/metadata console center, one log right.
CAN/I2C/Misc tabs are confined to the left INTERFACE SETTINGS column; fixed
SHARED IMAGE / METADATA and SHARED LOG headings identify the independent
shared panes. Tabs do not span their bounds or create duplicate resource pools.
CAN/I2C adapters retain their protocol-specific validation; future UART device
support must reuse this console. The last Misc tab is explicitly HOST BRIDGE
APP self-update over UART. Old hosts without the feature remain blocked.

F24 split projects are `firmware/RL78_F24_Bridge_boot` and
`firmware/RL78_F24_Bridge_app`; geometry is centralized at20KiB boot/app0x5000.
Post-build checks linker/IRQ/RAM bounds and creates a dynamic-length BUR1 app
BIN with trailing CRC32. These are build/mock-verified integration projects,
not physically verified releases. Install Boot once with a debugger/programmer;
then install matching App through Misc at115200, or initially program Boot MOT
plus App's packaged flash MOT together. `firmware/f24_board` is shared source
only, not a third product. See the
[shared-console/bridge guide](./docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md) and
[PDF](./docs/FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf).

The brighter overview separates HOST BRIDGE, DEVICE TARGET, live BOOT CODE,
live INSTALLED APP and offline LOADED APP BIN into five labeled cards with
bold primary values, measured wrapping and full-text hover hints. Green means
validated link/protocol data, blue means offline image, amber means reported
reference information, red means invalid/blocked, and gray means N/A. Live
metadata is not authentication or flash read-back. Custom Transfer reports no
boot/app versions. The [identity guide](./docs/UI_IDENTITY_GUIDE_zh-TW.md)
explains each source and the remaining screenshot/physical verification gates.

The CAN and I2C Master tabs share bridge probing, image, progress and logging.
Status cards use green/amber/blue/red/gray plus text for ready/required/working/
error/N/A. The bridge advertises selectable bus ports instead of the UI
guessing a board's port count. Current F24 provides CAN0 and IICA0 P62/P63
at100kHz; RA2L1/RA6 backends are future work. Existing CAN-only hosts retain
implicit CAN0 compatibility without I2C support.

I2C supports explicit-address G16 reference inquiry, signature/flash geometry,
and local CRC16 checks. It does NOT program the supplied G16 slave revision:
its active READ/auth and boot CRC/recovery gates need correction. No flash
erase/write/app jump command is sent. Raw G16 BIN has no embedded MCU/version/
start manifest. See the [multi-bus guide](./docs/MULTIBUS_TOOL_GUIDE_zh-TW.md)
and [PDF](./docs/FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf).

Settings use FirmwareUpgradeTool.ini beside the executable. A loading guard
prevents startup control events from overwriting stored settings. The rename
preserves the previously saved INI; the application never reads the legacy name.
The main window has no vertical scrollbar; current startup/tab/DPI
visual checks remain unverified because desktop automation was unavailable.

The layout repaint path clears exposed background and refreshes child controls
after tab changes and resize. This addresses old label/border pixels
from being retained by erase-free invalidation. Actual desktop visual regression
still needs confirmation; compile/protocol tests do not establish repaint PASS.

The COM selector displays `COM6: USB Serial Port (COM6)`-style Windows device
names so USB-UART adapters can be distinguished before opening a connection.
Names depend on the installed driver; unavailable descriptions fall back to the
COM number. The wider selector/dropdown preserves numeric port ordering and
the selected COM number on refresh. Connection buttons remain on the shared
top row. INI storage, serial opening, and reset/reconnect still use only
the canonical `COM6` value. `Open + Probe Host` remains necessary to identify
the actual MCU bridge; an adapter description is not firmware identity.

The Tool also provides a standalone bridge-board TX self test. It sends standard ID
`0x123`, DLC 8, `11 22 33 44 55 66 77 XX` once per second so a CAN analyzer can
verify host output without requiring a target response. `XX` resets to `00` at
each Start and increments for every transmitted frame. Select an advertised
250K, 500K (default), or 1M nominal rate; changing the dropdown sends
SET_CAN_CONFIG through the worker's binary UART link while CAN is stopped.
The self test uses that selected Classical rate. CAN FD uses the same nominal
choices with fixed 2M data. INI selection persistence is preserved.
This does not reconfigure a target: the current F13 firmware still requires
500K, and CAN Upgrade retains image/device metadata checks.

Development is in progress. The PC UI now uses a worker-owned Host Link v2
session for F24 host probing, exact-filter CAN setup, Custom Transfer discovery,
non-destructive transfer tests, and the metadata-driven CAN Upgrade workflow.
CAN Upgrade validates the complete image before START, keeps ENTER explicit,
and uses read-only app/boot identity plus bounded ABORT/CAN-stop cleanup. The UI
provides Classical/CAN FD preset display, validated profile/custom filters,
separate `Transfer Test` and fixed `Start Firmware Update` actions, a live list
of missing update prerequisites, optional detailed UART/CAN trace, log
recording, and an exportable session summary. The F24 host reports an automatic
firmware build number, supports one
active plus two pending generic CAN transactions, emits asynchronous CAN/error
events, and provides ACK-before-reset `RESET_HOST` with bounded PC reconnect.
P15/P16 carry binary Host Link only; F24 source and linker-map verification
confirm that no `printf` formatter is linked. The host startup clock is now the
internal HOCO at 40 MHz, so Host Link startup does not depend on X1/PLL lock;
after reaching `main`, bounded board-clock initialization switches the fitted
20-MHz crystal through PLL 80 MHz and `MDIV=1`, preserving a 40-MHz CPU/UART
clock and satisfying the CAN module clock requirements. Clock failure retains
the HOCO path and returns a specific initialization error before CAN starts.
CS+ preincludes the hand-maintained `f24_board_config.h`: BSP writes CMC once
in X1 mode, but X1 starts only after `main`. The same header limits nominal/data
SJW to 16/6 TQ without changing bitrates and forces the P10/P11 CAN route;
generated source stays untouched.
CAN CRXD0/CTXD0 are routed to the board's P11/P10 pins. Basic Classical 500K
Custom Transfer discovery, PING, ECHO, PATTERN_TEST, and GET_STATS have passed
on the F24/F13 hardware path. CLEAR_STATS, stress/error tests, other bitrates,
CAN FD and Boot/App updates remain hardware-pending.
Do not use the current binaries for a target firmware update until the
end-to-end checklist passes.

The current F13 build declares a 120-KiB application capacity, but accepts a
validated variable actual image length. Neither value is a tool-wide constant.
The128-byte format-6 core within the256-byte metadata region declares MCU identity, app start/capacity,
code-flash and boot-reserved sizes, actual image length, minimum size, erase
block, version/build, CAN capabilities and IDs. The UI displays those fields
immediately after file load, together with calculated/stored CRC32. Target INFO
pages report the live boot/app and layout metadata; programming requires the
two sources to agree. A future RL78/F14/F15/F24/F25/D1A, RA, or other
CAN-capable target can implement this same contract with different values and
does not require another MCU-specific UI profile.

Image inspection is cached when a file is loaded and revalidated once
immediately before START. Normal `UpdateUi()` calls do not rescan the image or
recompute CRC32.

## Confirmed communication settings

- Bridge Classical CAN: 250/500/1000 kbit/s; default 500 kbit/s.
- Bridge CAN FD: 250/500/1000 kbit/s nominal, fixed 2 Mbit/s data.
- Current F13 target projects: Classical 500 kbit/s only.
- Boot/app1.3.0 share the proven Transfer X1 4M /2 x16 -> CPU32/CAN16 baseline.
  Startup defers oscillator waits until main and keeps inherited PLL/CAN
  handoff state safe. Initially program the new paired boot_app.hex; do not
  leave a previous HOCO-only valid app behind after replacing only boot.
- On the QB-R5F10BMG-TB reference board, the TJA1050T CAN transceiver is powered
  from EVDD and requires a 5-V supply (4.75-5.25 V), even though the MCU can
  run at 3.3 V. P12 drives its S/silent-mode pin LOW for normal communication.
  Confirm supply and USB-UART logic compatibility before changing board power.
- F13 Boot request ID: `0x700`.
- F13 Boot response ID: `0x701`.
- Custom Transfer v1 request/response IDs: `0x710` / `0x711`. The
  `RL78_F13_CAN_Transfer` firmware is the reference diagnostic target and is
  flashed separately; its BIN is not loaded as a CAN Upgrade image. Its UART
  startup/error trace reports CAN initialization, compiled bus settings, and
  main-loop-decoded controller error flags for timeout diagnosis.
- Accept-all CAN receive filters are not supported.

## Components

- `src/`: Windows MFC application.
- `firmware/RL78_F24_Bridge_boot/`: the only F24 Boot product (UART recovery/update).
- `firmware/RL78_F24_Bridge_app/`: the only normal F24 bridge App product (CAN/I2C/Host Link).
- `firmware/f24_board/`: shared board/drivers/Smart Configurator sources, not a third flashable product.
- `docs/HOST_LINK_PROTOCOL_V2.md`: PC-to-host wire protocol.
- `docs/ARCHITECTURE_V2.md`: architecture and ownership rules.
- `docs/F13_TRANSFER_PROTOCOL.md`: deterministic F13 transport-test protocol.
- `docs/F13_BOOT_PROFILE.md`: CAN Upgrade v5 metadata contract and F13 baseline.
- `docs/F13_CRC_POST_BUILD.md`: CS+ post-build CRC insertion and independent
  artifact verification.
- `docs/F24_BRINGUP.md`: generated-driver wiring, board pins, build, and
  bring-up checks.
- `docs/DIAGNOSTICS.md`: log, detailed trace, and session-summary usage.
- `docs/TARGET_PROFILE_GUIDE.md`: adding a target without changing host
  firmware.
- `docs/HOST_BACKEND_GUIDE.md`: implementing a compatible RA2L1, RA6, or other
  generic MCU-host backend.
- `DEVELOPMENT_CHECKLIST.md`: ordered implementation and verification plan.
- `HANDOFF.md`: current development status.
- `SELF_CHECK.md`: verification gates.

The RL78/F13 transfer-test, bootloader, and application projects are maintained
as separate CS+ projects beside this repository.

## Requirements

### PC tool

- Windows 10/11 x64.
- Visual Studio 2022 with MFC support.
- Native C++17 build; no Python runtime is required by the released tool.

### RL78 firmware

- Renesas CS+.
- Project-selected CC-RL compiler.
- Smart Configurator for generated F24 peripheral configuration.

The current F24 host board schematic identifies `R7F124FLJ3AFB_C`. The CS+
project, `.scfg`, regenerated I/O header, and active `src/smc_gen` sources use
`R7F124FLJ3xFB`. A clean build succeeds, although CS+ still emits a historical
device-change/I/O-header notice because old FPJ5 metadata remains in inactive
project configuration records. See the F24 bring-up guide before regenerating.

## PC build

`Release|x64` is the delivery configuration. MSBuild writes it under
`tmp\internal\build\release\x64\`, then `scripts\build_mfc.ps1` copies the successfully
built EXE to `build\FirmwareUpgradeTool.exe` for delivery. Debug outputs are
isolated under `tmp\internal\build\debug\<platform>\`; Debug clean/rebuild therefore cannot
overwrite or remove the delivery EXE.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_mfc.ps1 -Configuration Release -Platform x64
```

The solution is `FirmwareUpgradeTool.sln`; the x64 Release executable is generated
as `build\FirmwareUpgradeTool.exe`.

Do not interpret a successful PC build as protocol or hardware verification.
See `DEVELOPMENT_CHECKLIST.md` for the complete release criteria.

The Host Link v2 codec tests can also be built and run independently:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'tests\HostLinkProtocolTests.vcxproj' /t:Build `
  /p:Configuration=Release /p:Platform=x64
& '.\tmp\internal\build\tests\x64\Release\HostLinkProtocolTests.exe'
```

## Documentation

- [Host Link Protocol v2](./docs/HOST_LINK_PROTOCOL_V2.md)
- [Architecture v2](./docs/ARCHITECTURE_V2.md)
- [Custom Transfer Protocol (F13 reference)](./docs/F13_TRANSFER_PROTOCOL.md)
- [CAN Upgrade v5 Contract and F13 Baseline](./docs/F13_BOOT_PROFILE.md)
- [F13 Boot/Update Flow Guide (繁中 Mermaid)](./docs/F13_BOOT_UPDATE_FLOW_zh-TW.md)
- [F13 Boot/Update Flow Guide (繁中 PDF)](./docs/F13_CAN_Bootloader_Update_Guide_zh-TW.pdf)
- [F24 Bring-up and Smart Configurator](./docs/F24_BRINGUP.md)
- [Diagnostics and Logging](./docs/DIAGNOSTICS.md)
- [Target Profile Extension Guide](./docs/TARGET_PROFILE_GUIDE.md)
- [Development checklist](./DEVELOPMENT_CHECKLIST.md)

The source-verified Traditional Chinese guide includes 16 Mermaid diagrams and
a 22-page visually inspected PDF with embedded fonts. Final release evidence
still requires physical-hardware traces and representative normal/error logs
and screenshots.

