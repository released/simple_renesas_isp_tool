# Simple Renesas ISP Tool

This personal project provides FirmwareUpgradeTool, a native Windows application for updating MCU
application firmware and testing communication through an MCU bridge.
It provides a shared interface for Classical CAN, CAN FD, UART, Xmodem and I2C,
with BIN inspection, live device information, progress, diagnostics and reports.

```text
PC Tool -> UART / USB VCOM -> MCU host -> CAN / CAN FD / UART / I2C -> MCU device
```

The Tool opens the host's COM port. The host forwards transactions through the
selected peripheral; the device's diagnostic UART can be observed separately
in a terminal. This repository includes the F24 (MCU CAN host) Bridge Boot/App
and common firmware sources for the device examples.

## Downloads and guides

- [Windows x64 executable](build/FirmwareUpgradeTool.exe).
- [Firmware downloads and file selection](https://released.github.io/slide_simple_renesas_isp_tool/#downloads):
  Host Boot/App, Device Boot/App and standalone Transfer images.
- [Tool operation guide](https://released.github.io/slide_simple_renesas_isp_tool/).
- [Bootloader and App authoring guide](https://released.github.io/slide_boot_loader_for_RL78/).
- [RH850 Bootloader and App guide](https://released.github.io/slide_boot_loader_for_RH850/).

The executable runs on Windows 10/11 x64 without a Python runtime.
The USB-to-UART adapter may require its manufacturer's driver.

## Functions

| Tab | Workflow / setting | Purpose |
| --- | --- | --- |
| CAN | CAN Upgrade; Classical CAN mode | Update a device App; examples cover F13 and F24 MCU CAN devices |
| CAN | CAN Upgrade; CAN FD mode | Update an F24 MCU CAN device App using FD+BRS and 64-byte data frames |
| UART / Xmodem | UART ISP | Update a device App using the framed UART ISP protocol |
| UART / Xmodem | XMODEM-CRC | Update a device App using ISP control and XMODEM-CRC 128-byte blocks |
| I2C | I2C ISP | Update a device App through the I2C mailbox protocol |
| CAN, UART / Xmodem, I2C | Custom Transfer / Xmodem Transfer | Test a matching standalone device's communication, echo and counters without programming Flash |
| I2C | G16 Reference (read-only) | Inspect the reference slave and its image CRC16; erase, write and App jump are disabled in this workflow |
| Misc | Host App update | Update the bridge's own App over the PC-to-host UART link |

UART, Xmodem and I2C device examples use independent Boot/App/Transfer projects
for F13, F24, G16 and G22. Each device Boot supports its selected update
interface/protocol. Wiring, configuration and hardware validation scope are
described in the [serial operation guide](docs/ISP_UART_TOOL_GUIDE_zh-TW.md).

The shared console provides:

- Separate Host, Device Boot/App and offline BIN information.
- CRC and metadata inspection, BIN Details, metadata comparison and Preflight.
- Image/target compatibility and version-policy checks, including an explicit
  [force downgrade operation](docs/UPDATE_VALIDATION_zh-TW.md#force-old-version).
- Staged progress, cancellation, exact error details and capability-based
  communication diagnostics.
- File logging, optional auto-scroll and HTML/JSON firmware update reports.
  **Open Last Report** opens the latest HTML report from the current session.

## Basic operation

For a Device App update, install its matching Boot with a programmer, then:

1. Connect the PC to the bridge and connect the bridge to the device using
   the selected interface. Follow the example's wiring and power requirements.
2. Run `build/FirmwareUpgradeTool.exe`, select the host COM port and use
   **Open + Probe Host**. Host Link starts at 115200 baud.
3. Select the peripheral, Workflow, port and communication settings.
   Device settings must match the device Boot/App and the loaded BIN.
4. Use **Load Update Image** to select the matching update BIN and review
   its metadata.
5. Use **Discover Target**. If discovery reports a
   running App, use **Enter Boot** before **Start Device Update**.
   A discovered Boot can proceed directly to update when its checks pass.
6. Review the completion result and firmware update report.

For communication testing, install the standalone Transfer image, select its
Transfer workflow, then use **Discover Test Target** and **Transfer Test**.
This workflow does not use a BIN loaded in the Tool.

Host App updating uses **Misc**, with its own **RUN** step. See the
[Host update guide](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md#bridge-update-example).

Step-by-step screenshots and field descriptions are available in the
[operation guide](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md) and
[BIN/live information and messages](docs/UPDATE_VALIDATION_zh-TW.md#tool-ui-fields).

## Firmware and connection requirements

- Initial Boot installation uses a programmer. App updates use the packaged
  update BIN for the selected workflow; programmer HEX/MOT files and standalone
  Transfer images are different file types with different purposes.
- Host App images are for **Misc**; Device App images are for the matching
  CAN, UART, Xmodem or I2C workflow. The Tool checks image role and route.
- A raw BIN alone does not establish target compatibility. The selected
  protocol/profile, live identity, metadata, Flash layout, IRQ contract and
  minimum Boot requirements must agree. See
  [image validation](docs/UPDATE_VALIDATION_zh-TW.md) and the
  [serial communication requirements](docs/ISP_UART_TOOL_GUIDE_zh-TW.md#serial-update-requirements).
- Changing a bridge rate or address does not reconfigure the device firmware.
  Use matching settings on both ends. Host Link baud and Device baud are
  independent.
- The F13 QB-R5F10BMG-TB CAN transceiver requires a 5-V EVDD supply.
  Check the board's supply and USB-UART logic-level requirements before wiring.

## Firmware projects and configuration

[F24 Host Boot](firmware/RL78_F24_Bridge_boot/) and
[F24 Host App](firmware/RL78_F24_Bridge_app/) share board, driver and library
sources in this repository. Device projects are separate checkouts;
each device project includes its own sources, configuration and build helpers.
The [download index](docs/MULTIBUS_TOOL_GUIDE_zh-TW.md#downloads) identifies
the corresponding source and installation/update files.

Device App configuration uses `app_image_config.h` and `app_irq_handlers.h`.
Project pre/post-build scripts generate and validate metadata, identity and
IRQ outputs. App builds also produce `DefaultBuild/app_build_report.html`
for inspecting the actual packaged BIN. Firmware authoring uses C headers,
Windows PowerShell and the MCU compiler; it does not require a JSON editor
or Python.

Successful Device App builds also produce FF-filled timing BINs in
`test_bins/auto/`. Open its `index.html` to choose a size; see
[App test images](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md#device-test-images).

F24/G16/G22 serial projects use project-local Smart Configurator settings;
F13 serial projects use Code Generator. See the
[SC configuration workflow](docs/ISP_UART_DEVICE_DESIGN_zh-TW.md#serial-scfg-workflow)
for Generate Code, Rebuild and I2C address representation.

| Topic | Guide |
| --- | --- |
| Device metadata, App IRQ mapping, Flash capacity and Boot installation | [Device workshop](docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md) |
| CAN protocol and byte/address reference | [Flash, metadata and CAN frames](docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md) |
| CAN FD operation and device configuration | [CAN FD guide](docs/CAN_FD_UPDATE_zh-TW.md) |
| UART, Xmodem and I2C device configuration | [Serial engineering guide](docs/ISP_UART_DEVICE_DESIGN_zh-TW.md) |
| Host App configuration and firmware build | [Host engineering guide](docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md#host-app-engineering) |
| PC-to-host transport protocol | [Host Link v2](docs/HOST_LINK_PROTOCOL_V2.md) |
| Adding a device profile or host backend | [Target profiles](docs/TARGET_PROFILE_GUIDE.md) / [Host backends](docs/HOST_BACKEND_GUIDE.md) |

## Build from source

PC development requires Visual Studio 2022 with the C++ and MFC components.
Run from this repository's root using Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_mfc.ps1 -Configuration Release -Platform x64
```

The solution is `FirmwareUpgradeTool.sln`; the delivery executable is
`build/FirmwareUpgradeTool.exe`. Building the PC solution does not build or
program the MCU firmware. RL78 firmware uses CS+, the project-selected CC-RL
compiler and its project README build instructions.

Repository layout:

- `src/`: Windows application, transport clients and update workflows.
- `firmware/`: Host firmware, shared drivers and device runtime sources.
- `docs/`: Operating guides, firmware authoring references and protocols.
- `scripts/`: Build, packaging, documentation and repository utilities.
- `tests/`: Automated software tests.
- `build/`: Executable and runtime files.

Repository publication utilities are described in the
[source/delivery publication guide](docs/GITHUB_PUBLISH_zh-TW.md).

