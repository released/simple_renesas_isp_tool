# Firmware Upgrade Tool

Native Windows tool and MCU-bridge firmware for CAN/CAN FD transfer testing,
target firmware updates and staged I2C master communication.

The separate [F24 MCU CAN device design](docs/F24_CAN_DEVICE_DESIGN_zh-TW.md)
uses an 8KiB Boot and App at 0x2000. Its Transfer/Boot/App builds and safe software models
are checked; F24 device hardware acceptance is still required.

## Downloads and operating guides

For editing device image metadata or adding App interrupts, use the
[F13/F24 CAN device workshop](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md):
editable versus generated fields, before/after examples, App-only commands
and BIN/vector verification. Both devices use app_image.json and
app_irq_handlers.json; generators own INC/header/vector outputs.

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
F13 and F24 CAN devices each maintain independent Transfer/Boot/App projects.

## Operation and firmware development

The Tool provides one shared image/metadata/log console. CAN and I2C settings
occupy the left TAB area; Misc updates the host's own App over UART115200.

- Host Link: powers up115200; compatible host Apps negotiate460800/921600.
- CAN: bridge self test, non-destructive Custom Transfer and device App update.
- I2C: explicit-address reference discovery and local CRC16 inspection.
  Programming is not enabled.
- Device images:256-byte metadata,128-byte core, reserved128FF, UPD1 policy,
  fixed complete IRQ forwarding and trailing CRC32.

Device App users edit two JSON files. Both F13 and F24 device pre-build and
packaging use Windows PowerShell and CS+/CC-RL. See the
[device workshop](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md) for metadata,
IRQ-key sources, App-local generation and binary checks.

Host firmware users can install Boot once with a programmer and update App
through Misc. See the [Host App guide](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md).
Communication rates, image route and live identity are validated independently.

Measured versions, hashes and hardware results are kept in the
[delivery record](docs/FIRST_DELIVERY_20261015_zh-TW.md).
Software checks and physical CAN/Flash/IRQ/fault acceptance are recorded separately.

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

The Traditional Chinese MD/PDF guides include diagrams, address tables and
source references. Hardware traces and post-reset App observations are
recorded separately from software-model results.

