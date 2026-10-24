# Firmware Upgrade Tool

PC Tool0109 keeps all existing functions with a smaller static x64 executable:
3,369,472 to1,291,264 bytes. Unused automatic MFC dialog Feature Pack support
is excluded; standard/custom controls and dialogs remain. No additional runtime
installation, packer or firmware/protocol change. Release/Debug and isolated
interface/paired-startup checks pass; physical device acceptance is separate.

The overview separates four live-board cards from the second-row loaded BIN
and12 key metadata fields. **BIN Details...** opens the complete read-only
20-field snapshot without requiring a connected board;all update checks remain.
The BIN card uses four summary lines with ellipses;hover shows the full text.

Serial device operation covers independent F13/F24/G16/G22 UART, I2C and
XMODEM-CRC128 Boot/App/Transfer projects. The **UART / Xmodem** tab offers
UART ISP, Custom Transfer, XMODEM-CRC and Xmodem Transfer under Workflow;
I2C ISP/Transfer are in I2C. Device UART0 is diagnostic text only.
Device baud is editable;I2C uses a selected7-bit address and advertised100/400K
clock. Match Boot/App initialization,live INFO and BIN metadata;see the
[short requirements table](https://released.github.io/slide_isp_tool_for_RL78/#serial-update-requirements).
See [serial operation](docs/ISP_UART_TOOL_GUIDE_zh-TW.md) and
[engineering/metadata contract](docs/ISP_UART_DEVICE_DESIGN_zh-TW.md).
F24/G16/G22 serial projects carry editable Smart Configurator settings and a
pre-build check of actual generated peripheral registers. G16 uses HOCO16;
the shared [SC workflow](docs/ISP_UART_DEVICE_DESIGN_zh-TW.md#serial-scfg-workflow)
describes Generate Code, Rebuild and I2C seven-bit/SVA address conversion.
Their build/model evidence is offline; physical board acceptance is separate.

Native Windows tool and MCU-bridge firmware for CAN/CAN FD transfer testing,
target firmware updates and staged I2C master communication.

For CAN FD500K/2M,BRS and64-byte block transfers,see the
[FD operation and device guide](docs/CAN_FD_UPDATE_zh-TW.md).
The F24 host App advertises FD burst bit13; its Boot is unchanged.
FD clean builds/software models and physical board acceptance are separate.
Custom Transfer supports Classical CAN and FD+BRS. Its
[FD communication test](docs/CAN_FD_UPDATE_zh-TW.md#can-fd-custom-transfer)
uses8-byte discovery/control plus a capability-gated64-byte ECHO test.
No firmware BIN is used; the standalone FD Transfer HEX is installed by programmer.
Its RXMB0 interrupt uses Renesas R_CAN_ReadRXMBInHandler and an8-entry RAM queue;
the main loop owns protocol/logging and TX completion remains polled. The
[FD guide](docs/CAN_FD_UPDATE_zh-TW.md#can-fd-custom-transfer) describes the
startup identifier and continuous-session checks beyond16/32/64 receptions.
For returned CAN timeouts, the [FD diagnostic fields](docs/CAN_FD_UPDATE_zh-TW.md#can-fd-transfer-diagnostics)
explain Host TX attempts/results and the device's bounded RXMB/TX snapshots.
The independent FD device Boot/App pair uses compact RXMB interrupt reception
and an8-entry queue with Boot-owned physical vectors and App-local ISR entries.
See [FD device receive/Flash flow](https://released.github.io/slide_boot_loader_for_RL78/#can-fd-device-rx).
Build/model checks and physical FD App-update acceptance remain distinct.
Normal FD Transfer and App->ENTER->update->APP_READY operation have board
evidence;the checked update has zero recorded Host error-counter increases.
Long stress and power/fault recovery remain separate acceptance items.

After END, the Tool uses a cancel-aware restart wait before read-only App
confirmation. Successful updates with increased Host error counters retain
an explicit warning in the completion feedback and JSON/HTML report, including
warning/error-passive deltas. See [confirmation and report semantics](docs/UPDATE_VALIDATION_zh-TW.md#app-restart-confirmation).

The separate [F24 MCU CAN device design](docs/F24_CAN_DEVICE_DESIGN_zh-TW.md)
uses an 8KiB Boot and App at 0x2000. Its Transfer/Boot/App builds and safe software models
are checked. Normal500K App programming, CRC, post-reset APP_READY/live INFO
and the incrementing UART timer counter are verified on hardware, using the
same Tool workflow as F13. See the [F24 device update result](https://released.github.io/slide_isp_tool_for_RL78/#f24-device-update-example);
other rates and fault/power recovery require separate hardware acceptance.

## Downloads and operating guides

For the 20 shared image fields, status colors, exact error messages and
CAN/Misc/I2C display differences, see the
[Tool field guide](https://released.github.io/slide_isp_tool_for_RL78/#tool-ui-fields)
or its [offline Markdown source](docs/UPDATE_VALIDATION_zh-TW.md#tool-ui-fields).

For editing device image metadata or adding App interrupts, use the
[F13/F24 CAN device workshop](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md):
editable versus generated fields, before/after examples, App-only commands
and BIN/vector verification. Both devices use app_image_config.h and
app_irq_handlers.h; generators own INC/identity/vector outputs.
The shared image contract is image_metadata_config.h (IMAGE_*), separate from
can_boot_config.h transport settings and boot_app_handoff.h RAM/owner markers.
Runtime metadata decoding/validation is independently compiled from the common
image_metadata.c/.h; image_metadata_profile.c supplies the project contract.
See the [C module porting entry](https://released.github.io/slide_boot_loader_for_RL78/#device-metadata-parser).
The workshop also covers [device250K/500K/1M configuration](https://released.github.io/slide_boot_loader_for_RL78/#device-can-bitrate),
[Boot/App ISR names and ownership](https://released.github.io/slide_boot_loader_for_RL78/#device-irq-ownership)
and [App UART tags/counters](https://released.github.io/slide_boot_loader_for_RL78/#device-app-runtime-log).
Each App post-build also produces DefaultBuild/app_build_report.html with the
actual BIN identity, metadata byte addresses, CRC/SHA-256 and complete vector
table. See the [build-summary guide](https://released.github.io/slide_boot_loader_for_RL78/#device-app-build-report).
Each RL78 CAN/FD App also carries a one-click FF-filled test-BIN generator;
see [App test images](https://released.github.io/slide_boot_loader_for_RL78/#device-test-images).

Device App version control is disabled by default in app_image_config.h.
When enabled, normal updates allow the same/newer version. The separate
force upgrade old version firmware action confirms one downgrade with a
public maintenance password; all identity/layout/CRC checks remain active.
See the [operation and password](https://released.github.io/slide_isp_tool_for_RL78/#force-old-version).

Start with the [public download and workflow index](https://released.github.io/slide_isp_tool_for_RL78/#downloads):
Tool EXE, F24 Host Boot/App, F13 Device Boot/App, six operating/flowchart entries
and the [Boot/App porting guide](https://released.github.io/slide_boot_loader_for_RL78/#boot-app-porting).
The matching offline source is [the multi-bus guide](docs/MULTIBUS_TOOL_GUIDE_zh-TW.md#downloads).
Initial Boot installation uses a programmer; Misc updates the Host App only,
while CAN Upgrade updates the external Device App. These images are not interchangeable.

Host App metadata and IRQ authoring also use C headers: app_image_config.h
and app_irq_handlers.h. The portable bridge_image_metadata.c/.h module,
engineering config/profile and RAM handoff are separate from the update engine.
See [Host App engineering](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md#host-app-engineering)
for editing, parser APIs and standalone App build/package checks.

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

One supported CAN update path is:

```text
Windows PC Tool -> UART/USB VCOM -> F24 (MCU CAN host) -> CAN -> F13 (MCU CAN device)
```

The PC Tool selects bridge ports from the connected host's advertised capabilities.
F13 and F24 CAN devices each maintain independent Transfer/Boot/App projects.

## Operation and firmware development

UART/Xmodem App and standalone Transfer may advertise Device UART diagnostic
capability. The Tool records RX errors/drops,TX timeouts and RX queue peak/pending
separately from bridge counters. Availability comes from live INFO,not the MCU
model. See [serial diagnostics](docs/ISP_UART_TOOL_GUIDE_zh-TW.md#serial-device-diagnostics).

The [metadata editor and cross-MCU/ISP portability design](docs/METADATA_EDITOR_PORTABILITY_DESIGN_zh-TW.md)
maps all metadata fields to their sources and Boot/App/IRQ/Tool dependencies.
It covers fixed-Boot App releases, MCU/interface migration and compatibility
acceptance; it is a design assessment, not an enabled editor or update adapter.

[Update confirmation and validation](docs/UPDATE_VALIDATION_zh-TW.md) covers
automatic App checks, metadata differences, staged progress, independent Cancel,
automatic JSON reports, diagnostics and the hardware acceptance matrix.
Verified updates show a completion dialog and persistent next steps. CAN stops
only after App confirmation and keeps Host COM open; Misc writing remains in
Boot until explicit RUN. Discover Target works in Boot/App; Enter Boot is
needed only for a discovered running App. See the
[completion guide](https://released.github.io/slide_isp_tool_for_RL78/#update-completion).
CAN mode/rate conflicts with the loaded BIN show a bold black `[BLOCKED]`
notice naming `Interface / Rate` and the settings to check, without suggesting
an unavailable Discover action. Existing discovery/update safety gates remain.
After discovery finds a running App, the available Enter Boot next step is
also bold black. Reconnecting resets the previous percentage/caption to zero
without deleting saved reports or resetting an active same-session operation.
CAN restart waits use App size and measured END CRC time, bounded to1..6s
inside the12-second App probe budget. ENTER uses the live installed size;
Cancel remains available. See the [restart confirmation guide](https://released.github.io/slide_isp_tool_for_RL78/#app-restart-confirmation).
Operation reports also include an offline HTML view; Open Last Report opens
the last HTML created in the current session. Preflight summarizes current
update gates without sending commands. Error Details preserves exact errors
with suggested checks; Windows PnP hints assist explicit COM selection without
probing or switching other ports automatically.

The Tool provides one shared image/metadata/log console. CAN and I2C settings
occupy the left TAB area; Misc updates the host's own App over UART115200.
Five framed, system-color MFC groups separate connection, BIN/live information,
peripheral settings, shared update actions and message log. Auto scroll down
defaults on and is stored in the INI; disabling it preserves the reading
position and selected text while new messages and file recording continue.
See the [console/log guide](https://released.github.io/slide_isp_tool_for_RL78/#tool-console-layout).
Step-by-step captures cover [COM connection](https://released.github.io/slide_isp_tool_for_RL78/#tool-connect-example),
[CAN device App update](https://released.github.io/slide_isp_tool_for_RL78/#device-update-example)
and [Misc Host App update/RUN](https://released.github.io/slide_isp_tool_for_RL78/#bridge-update-example).

- Host Link: powers up115200; compatible host Apps negotiate460800/921600.
- CAN: bridge self test, non-destructive Custom Transfer and device App update.
- I2C: explicit-address reference discovery and local CRC16 inspection.
  Programming is not enabled.
- Device images:256-byte metadata,128-byte core, reserved128FF, UPD1 policy,
  fixed complete IRQ forwarding and trailing CRC32.

Device App users edit two C headers. Both F13 and F24 device pre-build and
packaging use Windows PowerShell and CS+/CC-RL. See the
[device workshop](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md) for metadata,
IRQ-key sources, App-local generation and binary checks.
Both independent device projects are [golden samples](docs/DEVICE_GOLDEN_SAMPLE_zh-TW.md)
for future MCU authoring/build/validation flows. No Python is needed for firmware builds.

Host firmware users can install Boot once with a programmer and update App
through Misc. See the [Host App guide](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md).
Communication rates, image route and live identity are validated independently.

Measured versions, hashes and hardware results are kept in the
[delivery record](docs/FIRST_DELIVERY_20261015_zh-TW.md).
Software checks and physical CAN/Flash/IRQ/fault acceptance are recorded separately.

## Documentation maintenance

The common `/web` root owns the [public-content guidelines](docs/PUBLIC_CONTENT_GUIDELINES_zh-TW.md),
whose generated copy here defines
neutral wording, topic ownership, technical sources and release checks for
MD/web/PDF/README. The root policy is product-neutral and reusable by future
sites; both slide repositories include a synchronized local copy.

The [ISP guide](https://released.github.io/slide_isp_tool_for_RL78/) and
[Bootloader guide](https://released.github.io/slide_boot_loader_for_RL78/)
use short overview pages and separate topic pages. Existing homepage chapter
URLs continue to resolve through site-owned bookmark maps. Maintainer syncs
use scripts/web_guide_layout.json;canonical technical MD/PDF sources stay shared.

`docs` contains guides/specifications; `build` contains delivery/runtime files.
Reproducible intermediates and diagnostic evidence are kept under `tmp/internal`.

## Confirmed communication settings

- Bridge Classical CAN: 250/500/1000 kbit/s; default 500 kbit/s.
- Bridge CAN FD: 250/500/1000 kbit/s nominal, fixed 2 Mbit/s data.
- Current F13 target projects: Classical 500 kbit/s only.
- F13 uses the Transfer X1 4M /2 x16 -> CPU32/CAN16 baseline.
  Startup defers oscillator waits until main and preserves the PLL/CAN handoff.
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

The Host Link v2 codec tests can also be built and run independently from the
installed Visual Studio Developer PowerShell, in this repository's root:

```powershell
MSBuild `
  'tests\HostLinkProtocolTests.vcxproj' /t:Build `
  /p:Configuration=Release /p:Platform=x64
& '.\tmp\internal\build\tests\x64\Release\HostLinkProtocolTests.exe'
```

## GitHub source / delivery publication

Double-click `github_push.bat` to review, archive non-delivery DefaultBuild
outputs and commit/push F13 device Boot/App/Transfer, F24 Classical Transfer/Boot/App,
F24 CAN FD Transfer/Boot/App and Tool in order (ten repositories, twelve
DefaultBuild deliveries).
The Tool includes F24 CAN host Bridge Boot/App. Type PUSH to confirm; no force
push, automatic merge, build or web deployment. Local tmp/INI/log/report data
are excluded. Tool-side RH850 shared sources, guides and verification scripts
are eligible; independent RH850 and RL78 ISP MCU projects await hardware-board
validation and separate release authorization. Their project trees are excluded
from staging even if copied under Tool. Web publication remains separate.
For a read-only preview run scripts/push_github.ps1 without flags.
See the [publication and recovery guide (zh-TW)](./docs/GITHUB_PUBLISH_zh-TW.md).
The shared catalog is scripts/github_publish_catalog.ps1. Checkouts must
already have main, an initial local commit and matching origin; missing Git
setup stops before pruning. An existing empty origin supports initial main
publication. The script never creates/initializes repositories automatically.
Tracked or unignored local-only files are rejected before any output is moved.
Explicit staged removals of local-only tracking may remain; other pre-staged
changes still require review. Local files are preserved when using --cached;
the recovery guide explains the inspection and retry steps.

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

The Traditional Chinese MD/PDF guides include diagrams, address tables and
source references. Hardware traces and post-reset App observations are
recorded separately from software-model results.

