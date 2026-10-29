# AGENTS.md

## Workspace Layout And Safe Clean

Dedicated prerequisite fixture cleanup uses `scripts/clean_prerequisite_tests.bat`
(double-click applies; `--preview` is read-only). Its standalone PowerShell helper
and reviewed manifest retain unlisted/changed artifacts, INI, PNG/log/evidence;
only empty approved run directories may be removed. This separate lifecycle
entry does not extend normal clean or build/rebuild flows. New run directories
need ownership/hash review before adding them to the manifest. See HANDOFF for
the current manual-execution/automatic-review restriction before agent Apply.

Workspace profile exception: integrated PC/firmware/public-guide release; retain existing root RC/res and coupled diagrams/reference assets. Do not relocate frozen delivery files or publish raw captures for cosmetic uniformity. clean_mfc.bat uses clean_intermediates.ps1 scoped to tmp/internal/build, file allowlist only, reparse preflight; never purge tmp or obj/tests trees.


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
the Tool checkout's parent), in `slide_simple_renesas_isp_tool`; update
`slide_boot_loader_for_RL78` and `released.github.io` navigation when affected.
Keep features, protocol contracts, status/steps and screenshot gates consistent with MD.
Public web navigation/intros use evergreen function descriptions, not product
build numbers, delivery milestones, snapshot metrics or draft/publication labels.
Qualify current CAN example roles as F24 (MCU CAN host) and F13 (MCU CAN device).
F24 (MCU CAN device) is a separate 4MHz,8KiB Boot/App project and image contract;
do not infer role from MCU model or relabel legacy UART/I2C samples as CAN hosts.
Host App self-update remains Misc/UART, not CAN device programming.
Keep exact versions/hashes/measurements in release records; retain technical
protocol/image/IRQ compatibility and honest unverified-feature boundaries.
Regenerate affected guide PDFs directly in docs. Local synchronization does
not authorize Git commit/push, publishing, or fabricated UI/hardware evidence.

## Product boundaries

Device firmware examples each own their source/config/build tools; do not add
sibling Boot dependencies or common/version-number folders. Place function APIs
under hal/can, hal/device, hal/image, hal/flash and hal/isp as used by each role.
Flow orchestration is integrated in project-local custom_func.c hardware_init/loop;
maintainer API sync must not overwrite user workflow. Initialize derivatives
from independent golden samples with initialize_device_samples.ps1.
Firmware examples expose hardware_init()/loop() through project-local
custom_func.c/.h. Keep the selected main/startup adapter small and preserve
each role's initialization and return-to-App order. Backend runtime modules
may provide services, but do not define main or introduce transfer_main.c /
transfer_fd_main.c. Keep source bindings, preparation templates and isolated
SC regeneration checks aligned. Expand handwritten control flow and separate
statements across lines; do not compress if/else/for/while bodies.

F24/G16/G22 serial Boot/App/Transfer own same-name editable SCFG inputs.
Normal builds validate existing SC/actual registers and project-local peripheral
projection;never initialize/regenerate/reset user SC settings from pre-build.
Keep ISP_SCFG_REQUIRED/preinclude,16MHz G16 /32MHz G22,independent UART0 and
the actual timer/IICA source clock. G16 RFSP_Init uses16;IICA has no PRS bit.
Runtime owns bounded ISR/queue/mailbox/STOP/NACK/reset and fixed vectors;do not
link a second generated callback/vector/startup implementation. Initial profiles
and local validator are maintained in firmware/common/rl78_serial_scfg.
App-only projection stays in the selected App,never its paired Boot. F13 keeps
Code Generator;CAN golden and host/RH850 firmware are outside this SC change.

RL78 Xmodem local control-TX failure clears streaming and pending reset;
perform_action consumes it once. Preserve eight-KiB/min512spare. Actual RL78
runtime tests compile project globals/config and UART/I2C/Xmodem main loops
for all four MCU families and all three roles;do not substitute engine-only
coverage. Device diagnostic bit6/command25 is App/Transfer-only,optional and
read-only;separate Device lifetime snapshots from Bridge deltas. Maintain
isp_bus_contract/isp_uart_timing guards and RH850 generated-register baud
validation. sync_isp_runtime owns mirrors,not editable project configuration.
Actual SC tests use isolated copies and original-source manifests;projects
without .scfg are N/A,not simulated generation. Do not modify original audited
projects until source-preservation checks finish.

Device serial settings are editable,not Tool-fixed115200/I2C20/100K.
Queue isp::BusConfiguration(port,baud,address);edits invalidate discovery.
Bind selected rate/protocol to INFO and BIN;INFO has no I2C address field.
Selected7-bit08..77 must match BIN/readback address. Numeric UART bit31 and
300..2000000 limits are in device_uart_rates.h;bit0 remains115200 support.
F24 host App alone owns divisor/error<=2percent and IICA100/400K timing;
default device firmware and Host Boot/CAN stay untouched. Low baud uses
three-character/min5ms RX idle and byte-count+250ms deadline,max30s.
serial-update-requirements owns the brief public compatibility table;link it
elsewhere rather than duplicating. Hardware timing remains separately tested.

RH850 CAN/CAN FD/serial Boot/App/Transfer retain the complete original RH850
UART_printf retarget.c/.h feature set and call tiny_printf for foreground logs.
Do not replace it with the v4 compact formatter or strip float/ll/format support.
The formatter/project compiler uses C99; other handwritten modules retain C90
authoring style. Each project owns/registers local copies; putchar delegates to
the existing bounded SendChar UART0 hook, separate from ISP data.
Standalone MultiCHv4 uses the same retarget.c/.h/full.inc while retaining its own
board/UART guard. retarget_config.h owns editable TINY_PRINTF_FULL (default1),
not overwritten by API sync. Full uses C99 formats, fixed C locale/ASCII wide
conversion, IEEE32/64 and matching long double, nearest/ties-to-even rounding,
streamed precision and checked format/count/encoding/UART errors. Current0 keeps
the previous formats/rounding and attempted-output count. Preserve both modes;
do not claim hosted locale/fenv/stdio conformance. SendChar returns accepted byte
or -1 at its unchanged wait bound. Native v4 tests compile the formatter as C99
separately from strict-C90 CAN sources and retire fresh owned local outputs.
Do not add Device_Log/Device_LogHex/UartChar or manual numeric formatting again.
The canonical serial copies live in firmware/common/isp_rh850; API synchronization
preserves project custom_func workflows. test_rh850_console checks actual call
excerpts, byte formats/CRLF/counters, trace on/off and the finite busy-UART guard.

RH850 serial completion uses test_rh850_isp_runtime for all nine compiled roles:
actual RuntimePoll/config/Action/page/stream code, not a substitute decoder.
Keep reset gated by successful complete reply transmission; Xmodem control-TX
failure aborts streaming and clears the pending reset. Keep per-role state
declarations. verify_rh850_build_warnings permits reviewed license/alignment
categories only; never call these zero-warning target builds. Actual SC
roundtrip uses installed headless generation in isolated sibling copies and
compares original sources/App-only Boot hashes. Do not mutate originals in
parallel with that preservation audit. test_web_browser uses a private installed
Chromium profile and loopback server; RH850 preview routes are local only and
must not turn into public navigation/publication authorization.

RH850 serial preparation uses independent Sample_Project_RH850_S1_ISP_*
UART/I2C/Xmodem triplets;never overwrite the pre-existing basic UART/CAN/FD
projects. Keep R7F701684/16MHz/CPU120,64-KiB Boot,256-byte metadata,R8V1
entry/EBASE/full512INTBP validation,FCL-RAM and ECC-safe reader gates. UART
RLIN31 data and RLIN30 Log are independent;RIIC1 mailbox uses a latched NACK
until STOP and accounts for DRT prefetch before releasing a reply/reset.
Keep ISR/plain-helper separation;CC-RH rejects direct calls to interrupt
functions. All3 App-only builds must preserve paired Boot SHA manifests.
prepare_rh850_serial_scfg is explicit one-time initialization,not an automatic
build reset of user SC settings. No physical or publication acceptance is
implied by native models. RH850 has its own locally prepared Bootloader guide;
publication remains subject to the separate firmware acceptance gate.

Serial operation/engineering guides cover all four RL78 families and all three
independent UART/I2C/Xmodem contracts. sync_isp_uart_guide owns UART_TOOL_GUIDE
(ISP page) and SERIAL_DEVICE_GUIDE (Bootloader page); non-owner pages link.
Host/Device/offline BIN provenance is independent of result Tone. Misc uses
Host provenance for its live cards. Keep UART / Xmodem workflows discoverable,
natural-width tabs,live policy labeling and no-Flash Transfer gates.
test_isp_xmodem_client includes actual App/Transfer transport-loop CRC sinks,
not just Boot models. No hardware/publication is authorized by native tests.

Tool update reports are a worker-owned JSON/HTML pair, flushed to exclusive
partial files before non-overwriting publication. UI consumes paths/errors only;
never render reports from parsed/untrusted HTML or auto-launch a browser.
Preflight is a read-only UI/session snapshot, not permission to skip live worker
or device gates. Shared failure guidance preserves exact errors and offers
checks, never automatic erase/RUN retries. PnP instances remain local advisory
selection hints; hotplug refresh waits for idle/disconnected state and never
opens, probes or switches another COM. Do not add the metadata editor as part
of this Tool experience change. Native models do not establish UI/board results.

Prioritize F13/F24 MCU device Boot/App alignment and verification before F24
host work. Do not extend a device metadata/vector task into host firmware.

Update observability owns bounded CAN APP_READY/live-image confirmation and
explicit Misc RUN same-COM115200 revalidation. Never repeat erase/RUN to resolve
an unknown result. Keep independent Cancel worker-owned with ABORT/stop cleanup;
automatic reports include failed/canceled outcomes, phase durations and honest
counter reset/saturation/peak scope. Query live diagnostic capabilities rather
than guessing from model/version. No hardware evidence is implied by models.
See docs/UPDATE_VALIDATION_zh-TW.md; sync_update_evidence.ps1 owns its web block.
After END use DeviceRestartQuietMs(new image bytes,measured END CRC ms) and
AppConfirmationTiming/AwaitAppProbe:1000..6000ms without CAN requests,20ms
cancel slices,100ms between completed health probes,within the12-second probe
budget. ENTER uses live installed size/two estimated validation passes,not
the incoming BIN;unknown size waits6000ms. Keep size estimate8ms/KiB plus500ms
margin explicit as scheduling,not a hardware guarantee. Do not extend wire
timeouts,skip CRC,hide counter warnings or repeat END.
Host counter evidence includes warning/passive;increases on confirmed device
updates produce typed amber feedback/PASS_WITH_WARNINGS. Preserve unavailable,
failure/cancel and STOP-warning distinctions. Native timing/counter tests do
not establish physical restart-error elimination.
Successful update/RUN outcomes own one explicit dialog and persistent completion
guidance, never derived from a stage reaching100percent. CAN_STOP is sent only
after App confirmation; retain Host COM and clear live target only on STOP ACK.
STOP failure is App-confirmed/cleanup-warning with CAN state retained. Misc
write completion remains Boot/explicit RUN; failed or canceled operations never
inherit success feedback. Discovery is valid in Boot/App; UI ENTER requires
a discovered App. Do not turn a completion banner into write authorization.
Accepted explicit Open+Probe and a closed-to-open serial State reset the old
progress/caption/completion, not saved reports; same-session State must not
erase progress. Emphasize the actionable Enter Boot hint for a discovered App
only while idle and ENTER-enabled; image mode/rate blockers stay higher priority.

First engineering delivery is frozen at Tool0082/F24 Boot1.2.0-App1.5.0/F13
Boot-App1.5.0. User182253 verifies one normal115200/500K v5 update/END, not
post-reset App execution or full fault acceptance. See docs/FIRST_DELIVERY_20261015_zh-TW.md;
documentation-only delivery work must not rebuild/bump the measured binaries.

Current wire profile requires128-byte Host Link payload/frame142; no96-host
compatibility fallback. Canonical limits are firmware/common/host_link_limits.h.
CAN FD device updates use the independent RL78_F24_Boot_loader_CAN_FD pair:
500K/2M/BRS,FD64 raw0x720,FF-padded final block and format6/mode2 metadata.
Host App advertises bit13/command22 version2; bit12/version1 Classical stays
byte-compatible. UI/preflight/worker share can_host::BurstFeatureFor.
Host Boot and all Classical golden projects stay unchanged for FD work.
Keep FD build hooks/includes isolated,8KiB/512-byte spare and all image gates.
Custom Transfer accepts Classical or FD sessions without a firmware BIN gate.
FD discovery/base commands are8-byte FD+BRS; ECHO_FD64 command07 uses64bytes,
59echo bytes and CRC8, advertised by device capability bit6. Validate actual
ID/mode/BRS/wire-DLC/length before shared v1 parsing. Generic TX_WAIT_RX21 is
sufficient; do not require update-only burst bit13 or change Host firmware.
firmware/common/custom_transfer_fd.h is mirrored into the standalone FD
Transfer checkout; test_fd_transfer.ps1 verifies parity and actual C90 protocol
against the production PC client with trace off/on. Transfer MTPJ/hooks use
only FD helpers; explicit -Kind transfer never rebuilds Boot/App. Keep the
standalone programmer HEX distinct from an update BIN and hardware evidence.
FD Transfer STATE/IRQ/REGS snapshots are main-loop and Transfer-only;
retain the three-per-idle-episode/one-second bound and no automatic CAN reset,
FIFO drain or diagnostic flag clear.
Standalone FD Transfer enables only RXMB0 IRQ under F24_DEVICE_TRANSFER:
one64-byte RXMB0,exact0x710 rule,no RX FIFO. Unchanged vendor ISR acknowledges
RMND;callback uses R_CAN_ReadRXMBInHandler and an8-entry SPSC RAM queue.
No protocol/UART/TX wait in ISR;main-loop has no hardware polling fallback.
TX remains polled. Full queue drops newest;overwrites/errors drop that read.
Snapshots briefly mask/restore RXMB IRQ for coherent software counters,never
acknowledge flags/consume queue/reset CAN; preserve original mask even if1.
Diagnostics include IRQ/queued/dequeued/depth/peak/drops/API errors and RMIEC.
FD Boot/App use their compact RXMB0/no-vect Device_CanRxIsr and8-entry queue;
driver ISR generation0 does not disable hardware RMIEC/CPU IRQ. Boot owns4C
through AA/55 dispatcher;App maps4C locally. Keep actual compiled routing/RETI,
RX drop observations and512-byte spare guards. Boot replies are fixed standard
response ID/FD+BRS/eight bytes. Flash busy chain remains RAM-only with DI/EI;
queue does not buffer eight wire frames while global IRQ is disabled.
Preserve the validated Transfer preprocessed backend/HEX during Boot/App work.
Native compact-ISR/three-frame tests and hardware acceptance are separate.
Native Transfer tests extract the actual
vendor RXMB ISR/InHandler bodies/table/types; do not substitute an API stub
or claim physical IRQ acceptance from software models.
For CAN_TX_WAIT_RX21 TIMEOUT, preserve the raw error and validate its8-byte
attempt/result payload; classify it as bus transaction status,not missing UART
response. Include failed GET_STATS index without declaring a counter root cause.
docs/CAN_FD_UPDATE_zh-TW.md owns FD_TRANSFER/FD_UPDATE(ISP) and FD_DEVICE(Bootloader);
sync_can_fd.ps1 and -CheckOnly maintain both projections and topic links.
test_can_fd.ps1 takes explicit FD Boot/App paths and runs native models only.
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
Keep8KiB ROM and512-byte spare guards for the CAN golden samples. Serial ISP
G16/G22 projects use their own device SDK/Flash drivers and RAM budgets; do not
transplant F13/F24 hardware adapters or silently relax a new Boot partition.
CAN device UPD1 policy3 stores APP_IMAGE_VERSION_CONTROL at metadata116/0x74.
The valid installed App controls normal START:0 permits any version,1 permits
same/newer. Compare packed major/minor/patch, never build or Boot version.
Invalid-App recovery has no persistent version floor. INFO1D returns0/1/3.
FORCE_START0E bypasses only ordering for its own transaction; never arm or
persist an unlock. Tool's force upgrade old version firmware requires public
confirmation password3036, fresh live policy/version and normal metadata gates.
Do not log/store the password or advertise authentication. The maintenance
operation is canonical in docs/UPDATE_VALIDATION_zh-TW.md#force-old-version.
Device image magic is CAN plus NUL (0x004E4143), independent of CAN port.

### PC tool

- Keep the native system-gray/white five-group console. Analysis has four
  live cards on row1;row2 owns the offline BIN card/details button and three
  four-field metadata groups. Refresh all20 cached fields;show12 in the main
  view and all20 in read-only BIN Details. Details preserve current workflow
  guidance and labeled Device LIVE policy;never grant update permission or
  open COM,require no live target,and stay disabled while busy.
  Loaded BIN paints four complete rows with single-line detail ellipses;
  preserve full text/tooltips/details and verify actual font text height,
  including wrong-route diagnostics,not just non-overlapping rectangles.
  Status colors indicate results, not entire identity panels.
  Font-aware minimum height must keep1366-class normal/125-percent layouts
  within one screen; only log/dropdowns scroll. Log follow is ui/log_auto_scroll,
  default1; disabled appends preserve viewport/selection, not just the scrollbar
  style. Test the actual RichEdit helper and keep file recording independent.

- Native Windows MFC/C++17 executable; no Python runtime dependency.
- UART/USB virtual COM is the initial PC-to-host transport.
- Owns target profiles, image validation, workflow, user logs, and UI.
- Must identify and validate the MCU host before enabling CAN operations.
- COM discovery displays the Windows friendly name but stores/passes only the
  canonical COM name. Enumeration is read-only; never probe all listed devices
  or infer a bridge identity from a USB-UART driver description.
- Long operations run outside the UI thread.
- Worker notification is one outstanding wake;drain64 events on the40ms UI
  timer so posted trace cannot starve paint/input. Same-phase progress may
  coalesce,but retain phase/terminal order and explicit bounded-drop notices.
  Batch RichEdit color runs/scroll restoration and file flushes;preserve paused
  viewport/selection. Unknown waits use marquee without a fake percent,stop
  activity on failure/cancel,and keep completion/App gates independent.
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
- CAN/I2C/UART device adapters share one BIN/metadata/log/progress
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
  options/hooks/C headers. After SC generation reconcile projections before building.

- Each F24 split app owns app_irq_handlers.h and app_image_config.h. Run
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
  Host image contract is f24_bridge_common/image_metadata_config.h, with
  independently compiled image_metadata_profile.c. Common metadata decoding
  is bridge_image_metadata.c/.h, separate from command IDs/engine/Flash.
  Parse APIs require explicit extents; low-level Read/Write helpers require
  caller-validated windows. Keep BUR1/BUI1 bytes and accepted format unchanged.
  No-init cookie/owner belongs to boot_app_handoff.h. Post-build checks actual
  linked App identity as well as IRQ destinations; regenerated headers cannot
  legitimize stale compiled inputs. Use strict C-header readers, no JSON fallback.
  Prepare derived projects
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
  byte-compatible. Optional UART forwarding bit14/commands60..62 owns UART1
  P74/P75 with PIOR42=1, while Host Link UART0 remains independent. Preserve
  generic512-byte staging, exact immediate EXEC replay, RELEASE vs ABORT,
  bus reservations and bounded IRQ queues/counters. The raw UART client is
  offline-tested. UART tab/worker uses the native ISP target client, not a
  direct device COM; UART1 binary and UART0 Log stay separate. Xmodem remains
  a separate device project/protocol. See HOST_LINK_PROTOCOL_V2.md section17
  and docs/ISP_UART_TOOL_GUIDE_zh-TW.md. sync_isp_uart_guide.ps1 owns its web block.

### Target firmware

- RH850 CAN ISP now uses self-contained can_v4 mirrors: byte-identical v4
  service/driver,ISP-owned EVM/config/wrapper. sync_rh850_can_v4 checks the
  three owners without replacing user port/SC/metadata/startup inputs.
  Set-CanV4Project registers one driver only,with local includes before stale
  demo headers. CanPorts_Read calls Process but never drains IRQ FIFOs;
  reply success requires matching DONE,not queued. Preserve explicit channel
  controls,no auto restart/replay,retained hardware errors and protocol owner.
  test_rh850_can_v4 compiles actual core/config/driver/board/wrapper and is
  used by the existing multi-port/device/Transfer test runners.

- Each ISP device Boot project supports exactly one update interface/protocol.
  UART ISP, Xmodem and I2C have independent Boot/App/Transfer projects, not one
  multi-interface device Boot. Bridge host remains a generic multi-bus router.
  Device UART0 is dedicated diagnostic text (IspLog/printf routing); UART1
  carries only binary ISP/Xmodem. Never parse text or format logs in an ISR.

- Serial ISP common sources are mirrored under each independent Boot/isp.
  Keep8KiB/min512spare and App2000 for UART/I2C/Xmodem on all four families.
  RAM-only copy/equality helpers must retain exact bytes and bounded0..128
  extents, never substitute a hash for replay equality. Xmodem Boot has one
  fixed device/request/reply context and receiver/control-input union; do not
  allocate a second context or share it across physical buses. App/Transfer
  diagnostics remain available; Boot does not advertise STATS. Internal
  IspDeviceWrite/Finish share range/staged-metadata/Flash/CRC gates; Xmodem
  rejects framed WRITE/END. The verify adapter must check expected size,
  stored trailer, full CRC, metadata and all App IRQ slots before success.
  Run test_isp_transports and test_isp_xmodem_client for dynamic/direct/fixed
  binding regressions. G22 Xmodem copies RFD toFF420 (UART/I2C useFF400), with
  linker RAM/stack bounds checked; this is not a Flash partition change.

- RH850 has independent Classical and CAN FD Transfer/Boot/App projects.
  EVM R7F701684(100pins,1MiB),CAN1 TX P10_7/RX P10_6 Alt4,
  CAN4 TX P0_10/RX P0_9 Alt5,X1=16MHz,CPU120MHz,CAN40MHz;
  500K,FD data2M/BRS. ENABLE_PORT_CAN0..5 are numeric0/1,default1/4.
  Boot reserve64KiB with512-byte spare;App10000,metadata10100/256bytes,
  EBASE10200/512bytes,INTBP10400/512entries,code11000 onward.
  Keep neutral metadata/profile/parser,CAN config and handoff separate.
  Boot and App have independent full tables,EIRET ISRs and AA/55 handoff;
  App changes never derive Boot mappings or rewrite Boot products.
  Format6/policy3,valid-installed-App-only floor and transaction FORCE0E
  match the golden authoring/validation workflow. Do not add persistent policy.
  FCL App erase32KiB on first accepted CAN block;64-byte blocks accumulate
 256-byte aligned programming pages. ACK is RAM-session acceptance until
  page flush;last-block replay never erases/programs again. Guard actual FCL
  copy ordering/extent in28KiB RAM and safe reader in4KiB,plus debugger/cookie.
  Read unknown/blank Flash only through copied call-free RAM reader with
  saved/restored VCI ECC;do not describe it as hardware blank/ECC validation.
  tools/device_build.ps1 owns CS+ pre/post,actual metadata/512IRQ/map checks,
  BIN/HEX/HTML. App/Transfer own their HAL/config/build inputs and compile
  without a sibling Boot checkout; installed Boot/App contracts remain compatible.
  scripts/build_rh850_can_device.ps1 selects Kind explicitly;tests compile
  actual C90 engine/adapter and production PC client,max960KiB/replay/faults.
  docs/RH850_* sources own CAN/App/serial/porting/reference/timing topics;
  sync_rh850_device.ps1 projects them to slide_boot_loader_for_RH850 only.
  RL78/ISP/home use short linked entries; preserve the RL78 guide and aliases.
  User scoped this work to offline builds/models; no COM/programmer action.
  Keep RL78 golden projects and F24 host firmware unchanged.
  Three identical can_port_config/rh850_can_ports copies own independent
  FIFO[channel]/16-slot queues/15usable and channel-local TX0/status32*channel.
  Compact CPU storage only for enabled ports;no RXMB/commonFIFO allocation.
  RX/FIFO RAM budget21504bytes,worst FD all-six7296bytes;unique FIFO/rules.
  Shared EIINT23 round-robin drain has16-frame total budget and next IRQ.
  Transfer protocol contexts/counters/replay caches are per physical port.
  Boot staging RESET0A claims a bus;other buses remain discovery/INFO-only.
  Pre-START idle30s releases RAM ownership;active updates require owner ABORT
  or reset. Never merge raw blocks across buses or clear ever_started on ABORT.
  Full512-entry table defaults to one handler; never restore hundreds of
  duplicate dummy ISR bodies. OSTM0 channel84 supplies the1-ms tick.
  UART TX is optional/bounded with its CPU IRQs masked atomically after Start.
  No UART/protocol/TX wait in CAN ISR. Tests use the actual C90 protocol and
  queue adapter plus production PC client; mock API/register boundaries are
  not electrical or vendor-driver acceptance. Guard actual MOT pointers,
  R7F701684 FEDE0000 RAM ECC extent and generated comments. EVM helper keeps
  PLL480/CPU120/CAN40 from16MHz and excludes raw SC CGC in owned projections;
  App inherits clocks and supplies the SC synchronization variable only.
  CC-RH license warnings
  remain explicit; do not suppress/bypass them.
  The Transfer web block is canonical inside MULTIBUS_TOOL_GUIDE's
  TRANSFER_EXAMPLES; sync_transfer_diagnostics maintains it. No guessed repo URL.

- F24 CAN device source/build/model baseline is documented in
  docs/F24_CAN_DEVICE_DESIGN_zh-TW.md. It is not the 20MHz F24 host.
  Keep device Boot8KiB/App2000,512-byte spare,F2V1,targetF24B,last1KiB protected,
  complete App-local IRQ tables and RAM-only RFD. Normal115200/500K update,
  CRC/App confirmation and UART counter are user-verified; high-rate/fault
  acceptance stays separate. Synchronize
  its independent web block with scripts/sync_f24_device_design.ps1.
  Device repository READMEs are English. Keep Transfer/Boot/App reader structure
  parallel to the corresponding F13 READMEs, using actual F24 source facts and
  existing web anchors; do not copy hardware values or diagnostic capabilities.

- F13 image format6 uses a fixed complete normal-vector interface. Boot
  pre-build owns all04..7C stubs independently of app mappings; no competing
  -VECTN/C vect ownership. App handlers, including INTTRJ0, are normal RETI
  ISRs without vect. Preserve app_irq_handlers.h generation, app-local
  identity and artifact/real-HEX stack-model checks; never restore the old
  ordinary timer callback. Physical BRK7E is not forwarded. App-only IRQ
  additions within the fixed contract must not change boot sources/options.

- F13 boot/app share `f13_board_support` from the boot project: benchmark X1
  4 MHz -> PLL/CPU32 -> CAN16. Preserve the app startup's generated-user-area
  clock hook, deferred bounded waits, inherited PLL and CAN-reset handling.
  Do not transplant F24 clock or flash-library settings. EVB TJA1050T needs 5V.
- F13 Transfer provides deterministic non-destructive transport tests.
- F24 device Transfer uses main-loop raw C0ERFL capture (bits0..14), guarded
  by F24_DEVICE_TRANSFER; diagnostics must not change Boot/App image bytes.
  Preserve250ms UART trace throttle and optional trace-off counters. Canonical
  reader content is MULTIBUS_TOOL_GUIDE TRANSFER_EXAMPLES/TRANSFER_DIAGNOSTICS;
  retain F13 Classical/F24 Classical/F24 FD source and operation links in the
  Custom Transfer chapter itself. Synchronize ISP
  with sync_transfer_diagnostics.ps1 and -CheckOnly, plus its MultiBus PDF.
- F13 Boot/App provides the existing target boot protocol and image validation.
- Target profiles in the PC tool encode and decode target-specific CAN frames.
- F13 boot decision documentation is canonical in the marked section of
  docs/F13_BOOT_UPDATE_FLOW_zh-TW.md. Run scripts/sync_f13_boot_decision.ps1 to
  synchronize its full Bootloader section and ISP jump link; retain exact timer/ENTER/error semantics.
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

- PC0109 x64 Debug/Release uses `_AFX_NO_MFC_CONTROLS_IN_DIALOGS` only because
  this product uses standard CDialog/Win32/custom controls, not CMFC/Feature Pack
  controls. Keep static runtime, /O2 /GL /GS and security/exception settings.
  Native fixture compiler must match this macro. Reassess before introducing
  extended MFC controls; never remove real controls or change firmware for size.
  Detailed slimming measurements stay in tmp/internal, with concise current
  SELF_CHECK/HANDOFF records. Reuse unchanged device/guide evidence explicitly.
- github_push.bat / scripts/push_github.ps1 is the explicit maintainer publish
  entry. Plain PowerShell invocation is read-only. The entry selects exactly
  nine repos: F13 CAN and F24 Classical/FD device Transfer/Boot/App. The shared
  catalog's Tool/Host target is NOT selected; RH850/serial/web remain excluded.
  github_publish_release binds the reviewed source/delivery inventory to SHA256.
  Execute needs current-board confirmation or an exact AcceptanceFile. A narrowly
  user-authorized prior-board/offline-regression exception must keep
  BoardValidated=false,ExceptionAuthorized=true and actual evidence. Never infer
  an exception for a future snapshot. -Yes alone cannot establish either basis.
  User requests scripts/checks only: do not initialize/create/commit/push actual
  repositories without a separate request. Missing .git stops before pruning.
- DefaultBuild release pruning is a recoverable move to ignored tmp/internal/
  github_publish, never normal clean/build. Preserve the exact nine selected delivery
  files in the catalog; validate bounds/reparse points, remote ancestry and
  index before commit/push. Never force push, auto-merge or publish web sites.
  Reject tracked or unignored local-only data before archiving/staging. Permit
  pre-existing staged deletions only for the shared local-only path allowlist;
  source changes/deletions remain blocked. Never repair an actual index or
  remove tracking without user approval; preserve local files and index backups.
  Read docs/GITHUB_PUBLISH_zh-TW.md and select affected publication checks.
  Scoped-entry changes use verify_can_publication.ps1 (the old hold test name
  forwards to its GateOnly mode); owned fixtures retire on pass/fail. Run the workspace
  safe-clean audit when archive/deletion/clean behavior changes, not for this
  early refusal alone. No product rebuild/version bump for policy-only changes.
  Current user clarification2026-10-25: ALL MCU firmware-related content without
  real-board validation stays non-public, including Tool-side common/template
  source, dedicated guides, build/sync/test/mock assets and images. No exception
  exists for repository/folder, MCU family or Host/Bridge/Device/Boot/App/Transfer.
  Compile/model tests do not establish hardware acceptance; check actual-version
  board evidence and every proposed publication path. Unknown status is withheld.
  The existing six-family regex is only an additional legacy guard, not the
  publication boundary. The selected nine-project source/image snapshot needs
  evidence-backed acceptance before archive/index/commit/network actions;
  -Yes does not bypass it. Do not call lower-level
  publication helpers to evade the hold. Pure PC-tool/document changes require
  an explicit reviewed manifest excluding unvalidated firmware-related content.
  Previously published54Tool files are not automatically retracted by this
  correction; do not erase local files, rewrite history or change visibility.
  Public web projections of unvalidated firmware also remain deferred.

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

RL78 and RH850 Bootloader readers share scripts/bootloader_guide_contract.json:
equal chapter titles/order and mapped topic bodies. Maintain the paired topic
and specification when extending either guide; platform-specific values and IRQ/
Flash implementations remain independent. sync_rh850_device.ps1 projects both
guides' common navigation through bootloader_guide_navigation.mjs. Keep direct
homepage timing links and verify_bootloader_parity.mjs checks in the Web gate.
UART/I2C/Xmodem each own a Bootloader/protocol page with matching six-topic
structure in both platforms. ISP_SERIAL_TRANSPORT_GUIDE is the shared source;
serial_guide_content.mjs projects platform diagnostic limits and prepares the
combined existing serial PDFs. Keep common frame/INFO/status tables only in
serial-device; other pages link. sync_isp_uart_guide also refreshes the paired
navigation. Preserve moved legacy anchors and direct home flow/format links.

Web entry pages are short overviews;web_guide_layout.json owns each marker's
chapter destination. Get-WebGuideSectionPath is mandatory for synchronizers;
never hardcode index.md for complete chapters. Preserve guide-map.json legacy
aliases and refresh guide-routes.js when adding stable anchors. The optional
shared renderer data-markdown/guide:ready contract preserves default consumers.
Verify all chapter assets/markers and actual legacy destination IDs. Splitting
HTML alone does not require rebuilding identical PDF sources or MCU/Tool images.
Publish the shared notices_can renderer together with affected guide repositories
only when separately authorized. RH850 full topics belong to its independent
slide_boot_loader_for_RH850 site; RL78/ISP overviews carry short links only.

The canonical public-writing policy is /web/PUBLIC_CONTENT_GUIDELINES_zh-TW.md,
entered through the common /web/AGENTS.md. Keep it generic for every present
and future slide; MCU/project conventions belong here, never in root policy.
docs/PUBLIC_CONTENT_GUIDELINES_zh-TW.md is a generated standalone copy.
sync_public_content_guidelines.ps1 supplies identical Tool/slide copies;
run -CheckOnly and verify_public_content.ps1 after changes. Mechanical checks
do not replace contextual review or permit removing technical safety limits.

F13/F24 CAN device editing tutorial is canonical in
docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md (DEVICE_WORKSHOP block).
App-local generate_test_bins.ps1/.bat and device_test_bins_output.ps1 mirror
the canonical scripts via sync_device_test_bins.ps1;DeviceRoot selects all20
device Apps in device_app_catalog. Successful post-package gates generate a
bounded test_bins/auto set;manual batches stay unique and untouched. Replace
only exact hash-matched owned auto files;refuse unknown/modified/reparse paths.
Keep FF-only extension,source/version/IRQ/interface preservation,size/CRC
resealing,actual Flash/start/capacity and maximum-size coverage,ignored outputs
and PowerShell/.NET-only runtime. Never rebuild Boot/Host/Tool to generate inputs;
physical timing results remain separate from offline image validation.
sync_device_workshop.ps1 projects the full workshop only to the Bootloader page and a link on ISP; keep source C headers,
generated outputs, App-only commands and verification boundaries distinct.
Do not use host BUR1/Misc or host IRQ scripts for F24 CAN device examples.
The workshop owns device-can-bitrate, device-irq-ownership and App runtime-log
sections. Preserve separate F13 Boot/App RSCANlite timing inputs versus F24's
shared preinclude. Rate masks describe compiled timing, never dynamic rate
discovery. Separate-link ISR names may match; physical vectors stay Boot-owned.
F13/F24 device Boot/App are joint golden samples (docs/DEVICE_GOLDEN_SAMPLE_zh-TW.md).
Future MCUs/common changes start from both samples. Both devices use the same
six-field app_image_config.h plus app_irq_handlers.h X-macro; local identical
app_config.ps1 readers are mirrored from scripts/device_app_config.ps1.
Use sync_device_app_config.ps1 -CheckOnly. No device JSON input/fallback.
The same sync also owns identical local app_build_report.ps1 copies from
scripts/device_app_report.ps1. App post-build emits app_build_report.html only
after linked-image checks; report actual BIN bytes/hash, not predicted identity
or live hardware. Keep compile reports distinct from Tool operation JSON.
Both devices' ftable.inc / app_identity.h / app_irq_table.inc are generated, not
manual inputs. No INC fallback; preserve strict C-header/contract and stale linked
metadata checks before packaging. App-mode generation must not rebuild Boot.
Device image_metadata_config.h owns IMAGE_* format/MCU/Flash/policy/Boot
identity independently of CAN. can_boot_config.h owns CAN only and
boot_app_handoff.h owns RAM/owner. Build readers combine exactly those headers;
keep direct consumers, artifact guards, CS+ registrations and both golden
samples aligned. Do not restore metadata under CAN config or duplicate values.
Device runtime parsing/validation is independently compiled image_metadata.c/.h,
mirrored from firmware/common/device_metadata with sync_device_metadata.ps1.
Device sw_crc32.c traversal uses integer-address carry before far-pointer cast;
CC-RL far-pointer arithmetic wraps at64KiB. Keep F13/F24 Classical/F24 FD
copies identical and run scripts/test_device_crc_boundary.ps1 with all three
Boot paths plus real test BINs. Native flat-pointer CRC checks alone cannot
establish target address carry; retain the target-assembly guard.
Each project owns image_metadata_profile.c and config. Preserve C90/word-width/
endian/alignment/extent boundaries; decoding alone is not metadata/CRC validity.
Compact/detailed status changes labels only, never acceptance. Boot calls the
module directly; keep CAN staging/status mapping and CRC/launch logic separate.

Keep current user/developer guides and authoritative specifications in docs;
put historical verification reports, implementation plans and diagnostic
build/test outputs under disposable tmp/internal. Build contains only current
delivery/runtime files. No required source, manifest, firmware package or
user runtime log may depend on tmp surviving. Do not add another cleanup
verification report to docs; keep detailed internal evidence in tmp.

Firmware porting entry: docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md. Its marked
RL78_PORTING block is canonical for the boot web guide; run
scripts/sync_rl78_porting_guide.ps1 and -CheckOnly after related changes.
Public downloads and workflow entries are canonical in the PUBLIC_RESOURCES
block of docs/MULTIBUS_TOOL_GUIDE_zh-TW.md; sync_public_resources.ps1 updates both
web guides. Keep actual source/download URLs and stable section anchors tested. web_guide_sections.json
owns topic placement; non-owner pages contain short links, never full duplicates.
COM/device/host operation screenshots and their three TOOL_*_MEDIA blocks are
owned by SHARED_CONSOLE_BRIDGE_GUIDE. sync_tool_operation_media.ps1 copies only
docs/images/tool_operation public assets and validates block/hash parity for
16captures, including the two F24 device update-result PNGs. Their normal
Flash/App evidence is separate from Transfer-only and fault/power evidence.
Keep local paths redacted, actual states unchanged, originals separate, and
the ENTER inactivity/START boundary aligned with the actual device source.
Raw tool operation captures belong under ignored tmp/private_screenshots;
never substitute unredacted root captures for docs/images publication copies.
Custom Transfer captures use the MULTIBUS_TOOL_GUIDE CUSTOM_TRANSFER_MEDIA block
and docs/images/custom_transfer PNGs; sync_custom_transfer_media.ps1 validates
all four public assets and hashes before projecting to the ISP owner page.
Preserve screenshot states, distinguish cached OFFLINE BIN from live target,
and separate host counters from device GET_STATS. Normal-operation captures
do not establish physical fault/recovery or Boot/App Flash acceptance.
Companion F13 checkouts default to siblings; workspace_paths helpers support
explicit paths and machine-local environment overrides, never personal fallbacks.
Normal solution builds exclude the explicit F13 integration-test project.
Per-byte Flash/metadata and CAN payload reference is canonical in
docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md (FLASH_REFERENCE block).
generate_flash_reference.py derives all256 example byte rows from explicitly
selected real F13/F24 CAN device BINs without writing them; sync_flash_reference.ps1
updates the boot web guide. Keep relative-vs-absolute addresses, device manifest
layout, CRC scopes, source links and C90 examples distinct and verified.

Cross-layer teaching is canonical in docs/CAN_UPDATE_WALKTHROUGH_zh-TW.md
(CAN_WALKTHROUGH block); sync_can_update_walkthrough.ps1 updates the ISP web.
Keep Tool offline/live checks, generic host forwarding, device RAM/Flash stages
and post-reset App evidence distinct. INFO summaries are not raw metadata readback;
START does not erase F13 Flash. Link this tutorial before the byte lookup guide.

`README.md` must not contain local absolute paths, temporary debug conclusions,
or personal IDE settings. Detailed engineering state belongs in `HANDOFF.md`.

At release, provide verified Mermaid workflow diagrams and a visually inspected
Traditional Chinese PDF user guide as required by `DEVELOPMENT_CHECKLIST.md`.

