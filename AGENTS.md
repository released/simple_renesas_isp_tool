# AGENTS.md

## Purpose

Canonical workspace directory: `simple_renesas_isp_tool`. Preserve existing
FirmwareUpgradeTool product/project/EXE/INI and protocol identifiers. A folder
rename is not authorization to publish this project as a new GitHub repository.

This project builds a native Windows CAN/CAN FD and I2C firmware-upgrade
tool plus generic MCU bridge firmware. RL78/F24 is the first implementation.
RL78/F13 transfer-test and boot/application projects are the first targets.

The current user request, `DEVELOPMENT_CHECKLIST.md`, and the v2 architecture
documents are authoritative. Do not restore the removed Nuvoton Host Link v1
design or its target-specific bridge commands.

## Required documents

- `DEVELOPMENT_CHECKLIST.md`: ordered implementation and release checklist.
- `docs/HOST_LINK_PROTOCOL_V2.md`: PC-to-MCU-host wire protocol.
- `docs/ARCHITECTURE_V2.md`: layer ownership and dependency rules.
- `HANDOFF.md`: current implementation status and next work.
- `SELF_CHECK.md`: current verification gates.

Functional changes must update the relevant protocol/design document,
`DEVELOPMENT_CHECKLIST.md`, `HANDOFF.md`, and `SELF_CHECK.md` in the same change.
Also synchronize the affected web guide under `FIRMWARE_WEB_ROOT` (default:
the Tool checkout's parent), in `slide_isp_tool_for_RL78`; update
`slide_boot_loader_for_RL78` and `released.github.io` navigation when affected.
Keep features, protocol contracts, status/steps and screenshot gates consistent with MD.
Public web navigation/intros use evergreen function descriptions, not product
build numbers, delivery milestones, snapshot metrics or draft/publication labels.
Keep exact versions/hashes/measurements in release records; retain technical
protocol/image/IRQ compatibility and honest unverified-feature boundaries.
Regenerate affected guide PDFs directly in docs. Local synchronization does
not authorize Git commit/push, publishing, or fabricated UI/hardware evidence.

## Product boundaries

First engineering delivery is frozen at Tool0082/F24 Boot1.2.0-App1.5.0/F13
Boot-App1.5.0. User182253 verifies one normal115200/500K v5 update/END, not
post-reset App execution or full fault acceptance. See docs/FIRST_DELIVERY_20261015_zh-TW.md;
documentation-only delivery work must not rebuild/bump the measured binaries.

Current wire profile requires128-byte Host Link payload/frame142; no96-host
compatibility fallback. Canonical limits are firmware/common/host_link_limits.h.
Image metadata owns256 bytes at offset100h..1FFh,128-byte core plus128 reservedFF;
core/descriptor/payload/region/chunk sizes are independent. Bridge contract3
stages/reads64-byte pages with LE16 offsets; BEGIN erases only after complete
256-byte CRC/identity validation. F13 uses image format6, CAN protocol5.
UPD1 policy is shared at metadata96..127: image role, allowed update transports,
fixed IRQ ABI and minimum boot version. UI/worker/device reject wrong routes;
runtime app peripherals and PC-to-bridge UART do not define update permission.
F13 requires RESET0A/64 ordered METADATA08 chunks/SEAL09 before START.
CAN v5 uses64-byte blocks: BEGIN0B, raw standard IDs720..727, COMMIT0C/one ACK;
CRC/bitmap/metadata checks precede Flash. Last committed block replay never
rewrites. Generic bridge22/bit12 has no target-specific interpretation. Require
bit12 in UI/worker; do not restore v4 DATA03. Expose bounded log queue drops.
Keep8KiB ROM and512-byte spare guards; G16 new boot/app development is deferred.

### PC tool

- Native Windows MFC/C++17 executable; no Python runtime dependency.
- UART/USB virtual COM is the initial PC-to-host transport.
- Owns target profiles, image validation, workflow, user logs, and UI.
- Must identify and validate the MCU host before enabling CAN operations.
- COM discovery displays the Windows friendly name but stores/passes only the
  canonical COM name. Enumeration is read-only; never probe all listed devices
  or infer a bridge identity from a USB-UART driver description.
- Long operations run outside the UI thread.
- Serial RX uses documented Win32 first-byte return, not full-buffer/20ms
  timeout batching. Bound idle waits to the caller deadline/50ms; zero timeout
  only drains buffered bytes. Cache unchanged COM timeouts; preserve parser
  fragmentation/CRC, request retries/cancellation and write-timeout behavior.
  Win32 model tests do not establish FTDI latency or physical update speed.
- Settings loading guard is active from construction; EN_CHANGE must not save
  before the INI path exists or update partially created controls. Never call
  WritePrivateProfileString with an empty filename; keep real path/error logs.
- Footer progress captions must be single-line and measured using the UI font,
  reserving the100-percent suffix. Keep every footer child inside its band;
  operation-name changes may re-layout, percentage packets must not re-layout
  the whole window. Test long Misc/self-test labels, not only Progress0%.
- After child layout/visibility changes, erase exposed parent pixels and
  invalidate the child tree after final layout. The parent has no scrollbar;
  only the shared log history and dropdown lists scroll.
- CAN/I2C and future UART device adapters share one BIN/metadata/log/progress
  console. Misc is last and targets HOST BRIDGE APP over UART, not device flash.
  Confine the TAB strip to the left settings column; persistent shared-image
  and shared-log headings/panes must remain outside its horizontal bounds.
  It requires advertised bit10, a valid live descriptor and matching BUR1 BIN;
  no fallback to raw/CAN/G16 files or hardcoded PC-side F24 geometry.
- UI overview keeps host identity, live device, live boot/installed app, and
  offline loaded BIN separate. Never fill live fields from a BIN. Transfer
  does not report boot/app metadata; G16 versions are reported constants.

### MCU host firmware

- Only RL78_F24_Bridge_boot and RL78_F24_Bridge_app are buildable products.
  f24_board owns shared board/drivers/SC, never a third MTPJ or flashable image.
  App runtime belongs to App; shared UART/timer depend on minimal runtime hooks,
  not App headers. Prepare each existing MTPJ in place; preserve owned build
  options/hooks/JSON. After SC generation reconcile projections before building.

- Each F24 split app owns app_irq_handlers.json and app_image.json. Run
  prepare_f24_app_irq.ps1 in pre-build; do not derive its mapping from the
  canonical app's active IRQ list. App-only -AppProject build/package must not
  write boot files. Keep no-vect/global ISR, physical-vector/RETI and metadata
  guards. Reference SC projection preparation supports explicit -Kind app;
  default all is not a routine app update. Never reset a user's local mapping.

- RL78/F24 first; RA2L1 and RA6 later.
- Implements Host Link v2 and generic CAN/CAN FD/I2C transport only.
- App self-update command ownership includes INFO50/ENTER51/READ_METADATA57
  in the outer dispatcher, not only bridge_app.c. Reconnect must read/validate
  all metadata pages. Never workaround BAD_COMMAND57 by skipping validation.
- Split bridge boot/app additionally owns its own protected UART self-update
  contract (docs/BRIDGE_SELF_UPDATE_PROTOCOL.md), never target flash semantics.
  F24 layout is f24_bridge_common/f24_bridge_layout.h; prepare derived projects
  after SC regeneration. Keep explicit stack/handoff RAM, far-ROM constants,
  IRQ projections, RAM-only RFD execution and post-build packaging guards.
- Must not contain target flash addresses, F13 boot state, image layout, or
  target command interpretation.
- The Host Link UART is binary-only and must not carry `printf` output.
- APP Host Link powers up115200; bit11/commands05..07 negotiate460800/921600
  only while buses are idle. Flush old-speed ACK; confirm within2s or recover
  bootstrap, including TX stall. BOOT remains fixed and advertises no bit11.
  Never bind this session rate to a device BIN. DEVICE UART policy version2
  stores compile-time baud at metadata112; existing CAN/HOST version1 stays
  byte-compatible. UART device adapter/second-port firmware remains deferred.

### Target firmware

- F13 image format6 uses a fixed complete normal-vector interface. Boot
  pre-build owns all04..7C stubs independently of app mappings; no competing
  -VECTN/C vect ownership. App handlers, including INTTRJ0, are normal RETI
  ISRs without vect. Preserve app_irq_handlers.json generation, app-local
  identity and artifact/real-HEX stack-model checks; never restore the old
  ordinary timer callback. Physical BRK7E is not forwarded. App-only IRQ
  additions within the fixed contract must not change boot sources/options.

- F13 boot/app share `f13_board_support` from the boot project: benchmark X1
  4 MHz -> PLL/CPU32 -> CAN16. Preserve the app startup's generated-user-area
  clock hook, deferred bounded waits, inherited PLL and CAN-reset handling.
  Do not transplant F24 clock or flash-library settings. EVB TJA1050T needs 5V.
- F13 Transfer provides deterministic non-destructive transport tests.
- F13 Boot/App provides the existing target boot protocol and image validation.
- Target profiles in the PC tool encode and decode target-specific CAN frames.
- F13 boot decision documentation is canonical in the marked section of
  docs/F13_BOOT_UPDATE_FLOW_zh-TW.md. Run scripts/sync_f13_boot_decision.ps1 to
  synchronize both local web guides; retain exact timer/ENTER/error semantics.
- G16 I2C reference discovery/geometry and CRC16 are PC responsibilities.
  Its supplied slave lacks active READ/auth and safe boot CRC gating; do not
  enable erase/write/app jump or treat its label as authenticated MCU identity.
  See docs/MULTIBUS_BRIDGE_DESIGN.md and docs/MULTIBUS_TOOL_GUIDE_zh-TW.md.

## Hardware baseline

RL78/F24 Smart Configurator is the source of truth for device and peripheral
selection.

- CAN TX P10, CAN RX P11, CAN STB P12.
- UART TX P15, UART RX P16.
- SW1 P32/INTP7, SW2 P70/INTP8.
- LED1 P80, LED2 P81, LED3 P82.
- IICA0 P62/P63 is the initial I2C master backend (100kHz). Preserve the
  Config_IICA0_user.c IRQ hook; repeated-start needs the non-generated backend,
  not the generated callbacks that always send STOP. Port discovery and the
  Bridge_I2cBackend APIs are common; only board backends access registers.
- Classical CAN: 250/500/1000 kbit/s, default 500 kbit/s.
- CAN FD: 250/500/1000 kbit/s nominal, fixed 2 Mbit/s data.
- Bitrate configuration uses UART SET_CAN_CONFIG while CAN is stopped, not
  a CAN-500K bootstrap command. Validate advertised capabilities and retain
  target/image bitrate checks; changing the bridge never changes the target.
- Current host board revision uses X1/HXT 20 MHz. CS+ preincludes
  `f24_board_config.h` to defer X1 startup, constrain CAN SJW, and preserve
  CAN P10/P11 routing; do not remove
  that build setting or hand-edit generated files. Runtime uses PLL 80 MHz
  and MDIV=1, preserving a 40-MHz CPU/UART clock.

Regenerate Smart Configurator output after configuration changes. Keep custom
logic outside generated files except in preserved user-code sections.

## Coding rules

- Embedded hand-written C uses strict C90.
- Declare variables at the beginning of each block.
- Source-code comments are English.
- Do not add formatted output, blocking waits, or protocol parsing to an ISR.
- ISR code captures data/state into bounded queues or flags and returns.
- All wire lengths, indexes, DLC values, IDs, masks, and state transitions are
  validated before use.
- All queues are bounded and expose overflow/error counters.
- Avoid unrelated refactoring and generated-file reformatting.

## CAN filter policy

- No accept-all filter mode.
- Profile defaults use exact IDs.
- Custom masks must be non-zero and valid for standard/extended ID width.
- Filter configuration changes only while CAN is stopped.
- F13 Boot host RX default is standard ID `0x701`, mask `0x7FF`.

## Build and verification

- PC: Visual Studio 2022 with MFC, x64 Release is the release target.
- F24/F13: CS+ with the project-selected CC-RL toolchain.
- Do not treat old build artifacts as verification after generated source or
  configuration changes; perform a clean rebuild.
- Report changed files, change reason, verification method, and actual result.
- Do not claim hardware verification without a saved tool log/CAN trace.
- User authorizes automatically closing this project's running tool for a
  rebuild/delivery replacement; do not request closure again for routine locks.
  Match the executable's full path under this project's build or
  tmp/internal/build directory, not
  process name alone. Request normal close and wait for worker/COM/log cleanup.
  Stop the build on close rejection/timeout; never force-kill an updater or
  close other projects' tools, CS+, or IDEs. CloseRunningTool=false opts out.

The final PC product identifier is `FirmwareUpgradeTool`. Keep the solution, project,
executable, resource file, build scripts, window title, version symbols, INI
file, log prefix, and documentation aligned with that identifier. Do not read
or write the copied legacy INI filename.

## Public documentation

Keep current user/developer guides and authoritative specifications in docs;
put historical verification reports, implementation plans and diagnostic
build/test outputs under disposable tmp/internal. Build contains only current
delivery/runtime files. No required source, manifest, firmware package or
user runtime log may depend on tmp surviving. Do not add another cleanup
verification report to docs; keep detailed internal evidence in tmp.

Firmware porting entry: docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md. Its marked
RL78_PORTING block is canonical for the boot web guide; run
scripts/sync_rl78_porting_guide.ps1 and -CheckOnly after related changes.
Public downloads and six workflow entries are canonical in the PUBLIC_RESOURCES
block of docs/MULTIBUS_TOOL_GUIDE_zh-TW.md; sync_public_resources.ps1 updates both
web guides. Keep actual source/download URLs and stable section anchors tested.
Companion F13 checkouts default to siblings; workspace_paths helpers support
explicit paths and machine-local environment overrides, never personal fallbacks.
Normal solution builds exclude the explicit F13 integration-test project.

`README.md` must not contain local absolute paths, temporary debug conclusions,
or personal IDE settings. Detailed engineering state belongs in `HANDOFF.md`.

At release, provide verified Mermaid workflow diagrams and a visually inspected
Traditional Chinese PDF user guide as required by `DEVELOPMENT_CHECKLIST.md`.

