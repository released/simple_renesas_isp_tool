# Firmware Upgrade Tool Architecture v2

Status: implementation baseline

RH850 CAN/FD device triplets use a self-contained `can_v4` mirror of the
standalone v4 service/register driver,with separate EVM684 config/board/ISP
adapters. The protocol wrapper services foreground TX/status,never polls
IRQ-owned RX FIFOs,and accepts reply success only on matching DONE. Unconfirmed
TX faults that bus without auto restart/replay. Exact filters,source-port
responses,single-owner staging,metadata/FCL/CRC/vector/policy and independent
Boot/App tables remain protocol-owned. Hardware error evidence is retained.
See [RH850 CAN service](RH850_CAN_DEVICE_GUIDE_zh-TW.md#rh850-can-service).

The independent UART/I2C/Xmodem device projects are described in
[ISP_UART_DEVICE_DESIGN_zh-TW.md](ISP_UART_DEVICE_DESIGN_zh-TW.md).
F24/G16/G22 serial projects own their editable SCFG and actual generated
peripheral output. A local pre-build validator binds MCU/pins/clock/format,
UART baud or IICA SVA/rate to the C contract and emits a project-local header.
Runtime consumes those checked initialization values while retaining its
bounded ISR/queue/mailbox and startup/vector ownership. No generated callback
ISR or second startup is linked. Normal builds never reset or regenerate SC;
initialization profiles are explicit maintainer inputs. G16 uses HOCO16/RFSP16,
G22 HOCO32;F13 retains Code Generator. See the single-owner SC workflow there.
Device bus settings are an immutable queued ISP BusConfiguration(port,rate,
7-bit address). UI edits invalidate prior discovery;worker rechecks settings,
INFO and metadata before START. INFO has no I2C address field:the selected
transaction address binds the session. Numeric UART uses advertised bit31,
not Host Link baud negotiation;I2C clocks follow advertised port capabilities.
Low UART rates use a byte-count wire deadline and three-character idle floor.
It separates byte framing, update state, neutral metadata and RL78 board/Flash
adapters. The selected route is PC Host Link UART0 -> F24 bridge UART1 ->
MCU device; I2C devices use the bridge IICA0. No direct device COM is opened.
Optional bit14/commands60..62 implement generic UART staging and diagnostics;
the register backend owns UART1 only. The UART tab routes the worker-owned
native ISP client through that raw host client. It shares the image/metadata,
cancel/progress and report console, with full installed metadata and bounded
App-ready confirmation after END. Device UART0 remains diagnostic text; each
Boot project supports one update protocol. See [UART operation](ISP_UART_TOOL_GUIDE_zh-TW.md).
UART and Xmodem use separate workflows under UART / Xmodem; I2C ISP and its
Custom Transfer use the I2C tab. F13/F24/G16/G22 retain independent Boot/App/
Transfer projects for each contract. The G16 reference workflow stays read-only.
Xmodem App/Transfer command24 is a no-Flash CRC32 sink using the actual
main-loop receiver; Boot programming keeps its full-image checks and one EOT.
Native builds/client/register/frame models do not establish physical UART,
Flash, IRQ or reset acceptance. No device COM is opened directly by the Tool.

Device UART diagnostics are optional INFO capability0x40/command25,24bytes.
Only App/Transfer registers the read-only board callback;compact Boot retains
its existing capabilities. Worker logs Device observations separately from
Bridge UART1 deltas and retains them in report details after App confirmation.
RL78 Xmodem clears streaming/pending reset on local control-TX failure;only a
successfully transmitted final ACK permits reset. Actual RL78 runtime tests
cover all four families/protocols/roles,not just engine/receiver callbacks.

CAN FD updates use a separate device Boot/App pair with compiled FD-only
mode metadata. Host command22 version2/feature bit13 forwards three standard
FD+BRS records per64-byte block in101 payload bytes. The PC target profile
owns padding,CRC/retry and mode checks; the host remains generic.
Classical version1/bit12 and its device golden projects are preserved.
See CAN_FD_UPDATE_zh-TW.md; physical FD acceptance is independent of models.

Custom Transfer has an independent session mode in its PC client: Classical
retains its wire format; FD uses FDF+BRS with strict ID/DLC/flag validation.
The additive ECHO_FD64 command07/device capability bit6 checks59 echoed bytes
plus header/CRC8 in a64-byte frame. It uses generic TX_WAIT_RX,not burst/Flash
commands,and needs no Host App change. Its preflight does not require a BIN.
CAN_TX_WAIT_RX timeouts retain the raw host error and validate/decode its
attempt/result payload in the CAN client. Guidance distinguishes that returned
bus status from a missing PC/Host UART response. FD Transfer runtime snapshots
are main-loop and bounded; no diagnostic may pop/clear/reset CAN or
turn a partial transfer test into success. See the FD operation guide.
Standalone FD Transfer uses the unchanged vendor RXMB ISR at vector0x4C and
R_CAN_ReadRXMBInHandler in its callback. One64-byte RXMB0 and exact standard
0x710 filtering feed an8-entry SPSC RAM queue; receive FIFOs are disabled.
The vendor ISR acknowledges RMND; InHandler owns RAM-window/overwrite checks.
ISR work only captures/enqueues. Main-loop validates mode/DLC/length and runs
protocol/logging; TX remains polled. Full queue drops newest,not pending data;
head/tail publish only after complete copies. Snapshot acquisition masks/restores
only RXMB IRQ to copy coherent16-bit counters,never consuming/acknowledging data.
Fields distinguish IRQ/queued/dequeued/depth/peak/drops/overwritten/API errors.
FD Boot/App independently link a compact RXMB0 no-vect Device_CanRxIsr and
an8-entry queue. Driver ISR generation is off;RMIEC bit0/CPU source are enabled
explicitly after ready. Boot physical4C uses ownerAA dispatch;owner55 forwards
to App table slot2098. Post-build checks actual dispatch/App route/RETI and
rejects competing vendor vectors. Main loop owns protocol/Flash;RX drops and
sticky faults are observable. Boot TX uses compile-time response ID/FD+BRS/DLC8.
RFD busy work remains RAM-only under DI;ACK pacing,not RAM queue capacity,
prevents the host from streaming across a Flash busy window. All64 App BR
destinations remain validated after metadata/CRC by device_app_vectors.h.

The console uses five system-color MFC groups: Host connection, BIN/live-device
information, peripheral/target, shared load/update actions and message log.
Four live cards occupy the first analysis row. The loaded-BIN card and three
four-field metadata groups occupy the second row. All20 cached field values
remain refreshed. Loaded BIN paints heading/value plus two single-line detail
rows with ellipses,never a partial fifth line;full detail remains in window
text/tooltips and the read-only details snapshot. Other cards retain wrapping.
Twelve fields are visible and BIN Details exposes the complete read-only
snapshot,including explicitly labeled Device LIVE policy and route errors.
The details dialog does not open COM or grant update authorization and is
disabled while busy. Tabs own only peripheral settings. Geometry and font-aware
minimum height remain separate from protocol.
Provenance is independent of acceptance: Host live uses blue-gray, related
Device live cards use teal, and offline BIN/header groups use tan borders.
Misc live cards switch to Host provenance; errors retain their red status.
Device LIVE policy is explicitly labeled as live among offline BIN fields.
Log follow defaults on and persists as ui/log_auto_scroll in the portable INI.
The isolated RichEdit append helper preserves pixel viewport/selection when
follow is off, with a visible-line fallback for large histories. File logging,
worker queues, update acceptance and the existing text limit are unchanged.
CAN image mode/rate blockers take priority over discovery/ENTER/completion
instructions in the prerequisite card. Their explicit blocked/action text is
black and bold while retaining the error background/border. Returning to a
matching image, active operation or other workflow clears this emphasis;
button gates and all protocol/Flash checks remain unchanged.
Idle discovered-App guidance also emphasizes Next: Enter Boot only while its
button is enabled; entering Boot clears it. Explicit Open+Probe acceptance
and closed-to-open serial State clear stale progress/caption/completion.
Same-session State never resets progress, and saved report paths are retained.

Update completion now distinguishes END acceptance from post-reset APP_READY
and matching live identity/version/build/size. Misc END remains Boot; only an
explicit RUN triggers bounded same-COM115200 automatic App confirmation.
After CAN App confirmation, the worker requests CAN_STOP and retains Host COM.
END ACK starts a cancel-aware,size/CRC-dependent no-CAN-request restart wait
within the existing12-second probe budget. DeviceRestartQuietMs takes the
larger of ceil(image bytes/1024)*8ms and measured END CRC time,adds500ms,and
clamps to1000..6000ms. ENTER uses the live installed image size with two
size-estimated validation passes,not the incoming BIN;unknown size uses6000ms.
The8ms/KiB baseline is a conservative current-sample estimate,not an MCU speed
contract. Both use20ms cancel slices;health probes remain paced100ms after
completed attempts. This is PC scheduling,not a target startup guarantee;
transaction timeouts/retries,CRC/identity gates and counter warnings stay intact.
Counter evidence includes warning/error-passive deltas and distinguishes
unavailable snapshots from zero. A confirmed update with increased Host error
counters,or unconfirmed STOP,is PASS_WITH_WARNINGS with amber completion/report
feedback. Failure/cancel never inherits that success classification. Wire and
device/host firmware contracts are unchanged;see UPDATE_VALIDATION_zh-TW.md.
Only acknowledged STOP clears the live target snapshot; a failed STOP retains
CAN state and reports App-confirmed/cleanup-warning, not an update failure or
invented disconnect. Typed operation outcomes drive one completion dialog and
persistent status/next-step text, independently of stage percentages. Misc
written/awaiting RUN and RUN-confirmed are separate outcomes. Discovery works
in Boot/App; the UI enables ENTER only for an already discovered App.
Independent Cancel retains worker COM ownership and bounded cleanup. Shared
metadata differences, read-only preflight snapshots, stage progress, phase timing and automatic JSON/HTML reports
are PC responsibilities; host telemetry/stay reasons remain generic.
See UPDATE_VALIDATION_zh-TW.md. Device Boot8KiB/512spare and IRQ ABI are preserved.
Report rendering/UTF-8 flush/publication belong to the worker; UI receives paths
and errors only. Shared guidance preserves exact failures and suggests checks,
without changing protocol retry/erase/RUN policy. Windows PnP instances are
local selection hints; hotplug inventory refresh waits for idle/disconnected
state and never opens, probes or switches another COM automatically.

F24 MCU CAN device now has separate Transfer/Boot/App source/build/model
projects. It is not the F24 host backend:4MHz X1,8KiB Boot/App2000,targetF24B,
F2V1 IRQ ABI,RFD and protected final1KiB. It reuses CAN protocol5/image6 with
dynamic identity/layout; host forwarding and PC runtime are unchanged.
See F24_CAN_DEVICE_DESIGN_zh-TW.md. Normal F24 device Transfer discovery/test
captures are documented in MULTIBUS_TOOL_GUIDE_zh-TW.md; Boot/App Flash and
post-reset App confirmation/UART counter are verified for normal115200/500K
updates in SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md. Other rates and physical
fault/recovery remain separate acceptance gates.

0082 adds generic CAN_BURST_WAIT_RX22/feature bit12, compact standard Classical
records and nonblocking F24 progression. F13 v5 owns64-byte indexed data blocks,
CRC/replay/Flash semantics; the bridge does not. Same image format6,256-byte
metadata,8-KiB Boot/App2000 and IRQ ABI; F13 pair1.5.0 and F24 App1.5.0 required.
See CAN_BLOCK_TRANSFER_V5_zh-TW.md. UI queue overflow is now explicit and evicts
log events before state/completion where possible, never silently complete trace.

0080 changes only PC SerialPort read completion policy: Win32 first-byte mode
returns buffered/arriving bytes without waiting for the512-byte request to fill.
Idle waits are bounded by the caller's remaining deadline and50ms; zero timeout
is a nonblocking drain. Cache unchanged timeouts, leave write timeout/baud,
Host Link framing/CRC, retry/cancel and bus protocols unchanged. This common
transport benefits CAN/I2C/Misc without target-specific knowledge or a batch
command. See HOST_LINK_BAUD_zh-TW.md for baseline and physical A/B limits.

0077 adds optional APP Host Link baud negotiation (bit11 /05..07), isolated
from target protocol and port rates. Upper Host Baud selects115200/460800/
921600; boot self-update stays at its fixed descriptor rate. DEVICE UART
policy fixes its separate UART baud at compile time; UART ISP uses its own
image/transport contract and does not inherit Host Baud. See HOST_LINK_BAUD_zh-TW.md.

Host Link now uses shared128-byte payload/142-byte frame limits;96-byte hosts
are explicitly incompatible. Metadata Flash extent is256 bytes, independent
of128-byte defined core/descriptor and64-byte bus/metadata chunks. Bridge
contract3 stages complete metadata in RAM before BEGIN may erase; READ pages
are checked/reassembled and verified against live descriptors. F13 CAN image
format6 reserves the same region without moving code offset200h. See
COMMON_IMAGE_METADATA.md and BRIDGE_SELF_UPDATE_PROTOCOL.md.

2026-10-15 shared console extension: transport tabs own only interface settings
and actions. One frame-owned BIN/metadata/log/progress console dispatches to
CAN, G16 and bridge image adapters; no duplicated resource pool. No parent
scroll; pure console_geometry is tested at1366 desktop/minimum and larger text.
Misc targets HOST APP over UART and uses optional live bridge descriptors,
not PC-side MCU geometry switches. Portable C contract/parser/update engine
uses board flash callbacks; only F24 backend touches registers/RFD. See
BRIDGE_SELF_UPDATE_PROTOCOL.md and SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md.

0074 confines the TAB strip to the left INTERFACE SETTINGS column. Persistent
SHARED IMAGE / METADATA and SHARED LOG headings sit beside it, not inside a
full-width TAB surface. Existing controls/data remain frame-owned once; there
is no reparenting, duplicate BIN storage or change to bus/update contracts.

## 1. Objective

CAN device App authoring (F13 and F24 device, not F24 host) uses the same
six-field app_image_config.h plus App-local app_irq_handlers.h. Pre-build owns
generated metadata INC/identity/vectors; post-build rejects stale linked
metadata and verifies compiled IRQ destinations before packaging. Contract
fields remain Boot-owned; changing App headers cannot change device geometry,
transport or IRQ ABI. See DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md.

F24 host App generation is app-local: app_irq_handlers.h selects the
registered normal ISR symbols, app_image_config.h owns App release identity. The
pre-build generator uses only that app's MTPJ/sources plus fixed MCU/geometry
contracts, never another app's active IRQ list. App-only build/package take
AppProject; full reference SC regeneration remains explicit and separate.
Boot dispatch/metadata protocol remain unchanged. Packaging verifies every
linked slot and real RETI/RETB; see the shared-console guide's managed section.

Host metadata decoding is independently compiled bridge_image_metadata.c/.h.
Its config/profile, RAM handoff and command/Flash engine have separate owners.
Bounded byte parsing is shared by PC and MCU; full-image CRC and launch gates
remain in the engine/Boot. Post-build also compares linked release identity
with the App header. Host BUR1/BUI1 layouts and Misc115200 remain unchanged.

Provide one Windows tool that can use multiple MCU bridge boards to test CAN
and I2C transport and update supported target devices. RL78/F24 is the first;
RA2L1 and RA6 are future hosts. RL78/F13 transfer-test firmware and RL78/F13
boot/application firmware are the first targets.

## 2. Layer ownership

F24 has exactly two linkable products: `RL78_F24_Bridge_boot` owns recovery,
flash update and physical vector dispatch; `RL78_F24_Bridge_app` owns the
normal Host Link/CAN/I2C runtime. `firmware/f24_board` is shared board/backend/
Smart Configurator source, with no MTPJ or standalone firmware output.
Portable I2C staging belongs to `firmware/common`. Shared UART/timer code uses
`f24_runtime_hooks.h`, implemented independently by Boot and App; no Boot-to-App
source/header dependency. Each MTPJ owns its build settings. SC preparation
reconciles source registrations/projections without cloning a third product
or replacing App-local mapping/identity. Existing wire and image contracts
are unchanged. See `F24_BRINGUP.md` for regeneration and relocation steps.

```text
Windows UI and workflow
        |
Target profile and image validator
        |
Generic CAN host client
        |
Host Link Protocol v2 over UART
        |
Generic MCU host firmware (F24, later RA2L1/RA6)
        |
CAN or CAN FD bus
        |
Target transfer firmware or boot/application firmware
```

### PC tool owns

- The user-controlled periodic bridge-board TX self test. The worker configures
  Classical CAN at the selected advertised 250K/500K/1M rate and repeatedly issues the existing generic `CAN_TX`
  command; F24 remains unaware of test semantics.

- COM-port discovery and Host Link session management.
- Read-only Windows friendly-name enumeration in `serial_port_catalog`.
  Dropdown labels are presentation only; item data selects a canonical COM
  name for INI/transport/reset. USB adapter metadata cannot validate a host.
- Host identity, version, and capability validation.
- CAN mode, bitrate preset, and receive-filter selection.
- Worker-owned UART SET_CAN_CONFIG on mode/nominal dropdown changes while
  stopped. Capability filtering protects older 500K-only bridges. Bridge
  timing never implicitly changes a target or bypasses image/device metadata.
- Target discovery and target-profile selection.
- Target protocol command encoding/response decoding.
- Firmware file parsing, manifest validation, and update workflow.
- User-facing progress, logs, errors, cancellation, and reports.

### MCU host owns

- Bounded binary UART RX/TX and Host Link framing.
- CAN/CAN FD controller configuration using supported presets.
- Exact/masked hardware/software receive filters.
- Generic CAN TX, RX, wait-for-match, timeout, and retry transport.
- ISR-to-main-loop queues, controller error handling, and statistics.
- Board buttons and user-visible LED state indications.

The current F24 transport foundation uses independent 256-byte RX and TX UART
rings. UART ISR callbacks only enqueue bytes, record saturating error counters,
or complete an active TX span. Main-loop code starts each contiguous TX span.
The Host Link frame parser and dispatcher consume this transport but do not own
the UART peripheral directly. The Host Link UART is binary-only; `printf`
output is not routed onto it.

The current F24 control-plane dispatcher implements PING, host identity,
capabilities, statistics, statistics clear, host reset, CAN configuration, one
exact/masked filter, and CAN start/stop. It uses fixed buffers and a one-response
replay cache. Generic CAN TX and wait-for-RX use one active plus two pending
transactions with bounded TX/abort/response timeouts, duplicate-safe queueing,
ID/mask/flag/DLC matching, and up to three profile-selected retries.

CAN RX, state, and controller-error notifications are emitted as bounded Host
Link events. Error events include raw channel/global sources, controller state,
and cumulative arbitration-loss count. `RESET_HOST` queues its response, waits
until UART TX is idle, performs the F24 software reset, and relies on the PC to
close, reopen, and revalidate the same COM port for a bounded interval.

The F24 CAN receive boundary currently uses a four-slot ISR-to-main queue and
an exact default F13 Boot response filter (`0x701 / 0x7FF`, standard data).
Queue-full behavior drains the hardware receive buffer, discards that frame,
and increments a saturating lost-frame counter. CAN initialization leaves the
controller stopped with P12/STB high. `CAN_START` reapplies the pending filter,
drives STB low, and uses bounded transition polling; `CAN_STOP` is idempotent
and returns the transceiver to standby.

### Target firmware owns

- The F13 boot/app pair shares board clock/CAN preparation from
  `f13_board_support` in the boot project. The verified Transfer benchmark
  uses X1 4M/PLL32/CPU32, CAN16/500K. Bounded main-loop clock startup gates
  CAN/FSL; app startup preserves a valid inherited PLL and explicitly resets
  inherited CAN modes before configuration. The generated app user-area hook
  must survive regeneration. Flash/vector metadata and IDs are unchanged.
- Its own device identity and target protocol.
- Validation of command state, sequence, target, size, and data.
- Transfer-test responses or bootloader flash operations.
- Safe behavior after timeout, reset, communication loss, or partial update.

## 3. Dependency rule

Target workflows may depend on the generic CAN host interface. The generic host
interface and host firmware must never depend on an F13 flash layout.

Adding an RA host changes the host firmware/backend only. Adding a target that
implements CAN Upgrade v5 changes target firmware/metadata only; an incompatible
wire protocol requires a separate PC workflow.

Image capacity and application start are never global tool constants. The
format-3 application manifest and CAN Upgrade v5 INFO pages declare size range,
MCU/layout identity, integrity algorithm, address range, CAN capabilities and
wire protocol. The current F13 build declares a 119-KiB capacity; another target
can use different values without adding an MCU-specific UI profile.

## 4. PC module target structure

```text
src/
  app and UI
  worker_session
  core/
    host_link_protocol
    host_link_client
    serial_port
    serial_port_catalog
    can_host_client
    firmware_update_profile
    f13_boot_profile
    f13_transfer_profile
```

The checked-in `host_link_protocol` module is the v2 wire-format codec and
bounded stream parser. `host_link_client` owns request sequence allocation,
byte-identical timeout retry, response correlation, bounded event routing, and
host identity/capability parsing. It depends on a small byte-transport interface;
the Windows `SerialPort` implements that interface. `can_host_client` validates
and encodes generic CAN configuration, filters, frames, TX, and TX-wait-RX
without target-specific knowledge. `worker_session` is the sole runtime owner
of the serial handle and the Host Link/CAN/target clients. It executes host
probe, CAN setup, Custom Transfer discovery/tests, and CAN Upgrade updates on one
worker thread, then posts immutable state/progress events to the MFC UI.
The copied Host Link v1 bridge/update modules are removed from this project.
`f13_transfer_profile` is the first target-specific codec/client. It owns the
fixed eight-byte Custom Transfer frame, CRC-8, response validation, multi-page
device-identity assembly, target sequences, and non-destructive command
workflows while depending only on the generic CAN client.
`f13_boot_profile` currently implements the generic format-3 manifest/CRC
validation, paged read-only INFO identity, and strict CAN Upgrade v5
HELLO/ENTER/metadata/START/BLOCK/END/ABORT transactions.
Discovery reads INFO from either application or bootloader and uses HELLO only
after boot state is confirmed; ENTER remains an explicit UI action. After
START, cancel/error cleanup attempts target ABORT and CAN_STOP before COM
teardown.
`firmware_update_profile` is the UI-facing image-inspection boundary. It returns
normalized identity, version, size, CRC, flash layout and CAN information from
the image metadata. The generic UI does not contain the F13 app start, capacity,
image size, or flash end address.

The exact filenames may change during implementation, but these responsibilities
must remain separate.

### UI execution rule

0079 keeps the settings restore guard active from construction; initial
EN_CHANGE notifications cannot persist empty/default controls before the INI
path is resolved. Writes also reject empty paths; real OS failures remain
visible. F24 App1.4.1 routes READ_METADATA57 through its outer Host Link switch;
descriptor-only reconnect without installed metadata validation is not allowed.

0078 footer layout measures the operation caption with the actual UI font and
reserves its complete `: 100%` suffix. The progress label is explicitly
single-line; label/bar/elapsed/build rectangles stay inside the footer band.
The bar fills the remaining width instead of starting at a fixed145px label.
Only a changed operation name triggers re-layout; percentage packets do not.
GDI metrics at100/125/150-percent fonts and supported widths are regression
tests, not proof of the actual monitor/DPI rendering or an update on hardware.

After the final fixed-coordinate layout and scrollbar-position clamp, the main
frame invalidates its background and complete child tree with
RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME. This covers TAB
visibility changes, scroll and resize despite WS_CLIPCHILDREN. Paint remains
deferred/coalesced; the intermediate clamped layout does not redraw twice.
OnVScroll uses this common path, not an additional Invalidate(FALSE). No bus,
metadata, image CRC or worker state changes are part of this painting policy.

Long-running host discovery, target discovery, transfer, and update work runs on
a worker context. It reports immutable progress/events to the UI thread. The
workflow must not pump nested window messages from inside an update loop.

`Transfer Test` and `Start Firmware Update` are separate controls. The firmware
button remains visible with a fixed label and is enabled only when the F24 host
is validated, `CAN Upgrade` is selected, selected CAN settings are supported by
the loaded image, the discovered target is a validated compatible bootloader,
no operation is active, and a locally valid update BIN is loaded. A nearby
status line lists every missing prerequisite or reports `Ready`; clicking the
enabled button still requires explicit confirmation before START.

Image validation is cached when a file is loaded and recalculated only when the
selected workflow changes. The click handler performs one final validation
immediately before START. `UpdateUi()` only reads the cached result, so routine
window/state refreshes never rescan a large image or recompute CRC.

### Session state

The UI overview presentation model consumes immutable worker HostInfo,
Transfer DeviceInfo and CAN Upgrade TargetInfo snapshots, plus cached local
image inspection. It never infers MCU identity from COM descriptions or
projects an offline manifest onto live device cards. Host/target validity
gates hide old metadata on disconnect; I2C port/rate/address changes invalidate
the displayed reference snapshot. Cards report protocol metadata, not flash
read-back or firmware authentication. G16 reference versions remain amber.
Overview rendering/layout is split into main_frame_overview and ui/overview_model;
CRC, safety gates, wire formats and MCU firmware are unchanged.

```text
DISCONNECTED
  -> SERIAL_OPEN
  -> HOST_VERIFIED
  -> CAN_CONFIGURED
  -> CAN_RUNNING
  -> TARGET_IDENTIFIED
  -> TRANSFER_ACTIVE
  -> TARGET_IDENTIFIED or ERROR
```

Closing COM, host reset, protocol incompatibility, or unrecoverable serial
failure returns to `DISCONNECTED`. CAN bus-off and target timeout are distinct
errors and must not be displayed as a generic COM failure.

The UI never closes the serial handle while a worker transaction is active.
Cancel/disconnect sets a cancellation flag; Host Link reads are sliced to a
maximum of 50 ms while cancellation is enabled. The worker then stops CAN and
closes COM from the same ownership context. During an F13 update, bounded
ABORT/CAN_STOP cleanup runs before COM teardown where protocol state permits.

## 5. Identity model

Host and target identities are independent.

### Host identity

- Host board ID: F24, RA2L1, or RA6 implementation.
- Host firmware semantic version and build number.
- Supported Host Link protocol range.
- CAN modes, bitrates, maximum payload, filter, queue, and timestamp features.

### Target identity

- Family: RL78/F1x, RL78/D1A, RL78/F2x, or another supported family.
- Exact logical target/product identifier.
- Target protocol and image-format versions.
- Running state: application, bootloader, or transfer-test firmware when known.
- Application/boot firmware version when available.

The tool must not infer a target only from a CAN response ID.

## 6. Initial profiles

### Custom Transfer workflow (RL78/F13 reference)

Purpose: prove PC-to-host-to-target transfer integrity before flash update.

Required target commands:

- `DEVICE_INFO`
- `PING`
- `ECHO`
- `PATTERN_TEST`
- `GET_STATS`
- `CLEAR_STATS`

- Classical CAN at 500 kbit/s.
- Request ID `0x710`; response ID `0x711`.
- Exact standard-data filters use mask `0x7FF`.
- Fixed eight-byte requests and responses use protocol version 1, a non-zero
  sequence, three command bytes, and CRC-8/ATM.
- Device identity is reported by the target. The PC accepts any non-reserved
  target ID and valid one-to-eight-character ASCII label; it does not hard-code
  `0xF13B` or `R5F10BMG` for this diagnostic workflow.
- No firmware BIN, image manifest, flash geometry, or erase/write command is
  part of Custom Transfer. The F13 project remains the first reference target
  and transport-isolation fixture.
- The F13 main loop implements duplicate replay and bounded TX/abort handling;
  CAN error ISR work is limited to saturating counter updates.

### CAN Upgrade workflow (current F13 implementation)

Purpose: update an application through CAN Upgrade v5 using matching image and
device metadata.

- Classical CAN at 500 kbit/s after all F13 projects change together.
- Request ID `0x700`.
- Response ID `0x701`.
- Current F13 target ID `0xF13B`; this is metadata, not a UI profile constant.
- PC validates the complete update image before START.
- PC compares the 128-byte image manifest with all device INFO metadata pages.
- Blocks use24-bit byte offsets,64-byte RAM staging and8 indexed raw CAN IDs.
- F24 transports frames but does not interpret HELLO/START/BLOCK/END.
- Target remains the final authority for manifest, flash, and CRC validation.

## 7. Filter policy

- There is no accept-all mode.
- A metadata/workflow default uses an exact ID mask.
- Custom standard and extended filters validate ID/mask width.
- A zero mask is rejected on the PC and host.
- Filters change only while CAN is stopped.
- The GUI shows effective ID, mask, IDE, and frame type before CAN starts.

## 8. F24 board behavior

- CAN TX P10, CAN RX P11, CAN STB P12.
- Host UART TX P15, RX P16.
- SW1 P32/INTP7, SW2 P70/INTP8.
- LED1 P80, LED2 P81, LED3 P82.
- IICA0 SCL P62/SDA P63 provides the generic100/400kHz I2C backend(default100). The preserved
  user ISR hook dispatches to Bridge_I2cBackend_Irq; generated callbacks are
  not used for owned transfers because they cannot retain repeated-start.

The additive multi-bus layering, port contract and G16 safety boundary are
defined in [Multi-bus design](./MULTIBUS_BRIDGE_DESIGN.md). UI -> worker ->
target protocol -> CanHost/I2cHost -> Host Link -> common bridge APIs ->
board backend. Only the final backend accesses F24 registers or future RA FSP.
CAN and I2C serialize; changing a selection never silently repurposes an active
bus. Raw G16 BIN metadata is not fabricated from live target geometry.

Interrupt handlers capture data/state only. Parsing, button debounce, LED state
transitions, Host Link dispatch, and binary diagnostic-event construction run
outside ISR context. The F24 host contains no `printf` call or `putchar`
retarget, so P15/P16 cannot be polluted by application text output.

LED priority:

```text
ERROR > TRANSFER_ACTIVE > TRANSFER_START > HOST_CONNECTED > IDLE
```

## 9. Reliability rules

- All queues are bounded and expose overflow counters.
- Hardware, mode, and TX waits are bounded. FSL uses internal automatic status
  mode and reports its final result. Neither F24 nor F13 uses WDT; recovery uses
  explicit error paths, reset, boot retention, and image validation.
- Host duplicate request replay never repeats a completed CAN side effect.
- Target retry policy is selected by the active target workflow.
- Update cancellation and communication loss leave F13 recoverable in boot mode
  if its application may be incomplete.
- A target image never runs unless its manifest and CRC pass target validation.

## 10. Open decisions

- Maximum acceptable full F13 update time.
- Exact button actions beyond diagnostic input reporting.
- Error-indication clearing policy after reconnect/reset behavior is tested.

The security boundary is CRC32 integrity without firmware authentication or
digital signatures. Device policy3 optionally rejects lower App versions;
Tool confirms a request-local FORCE_START with a public maintenance password,
not a persistent unlock or authentication. See UPDATE_VALIDATION_zh-TW.md.
The device bootloader still requires valid identity, manifest, and full
image CRC before entering the application.

## 11. Independent serial ISP device contracts

Each device Boot selects one update protocol. UART0 diagnostic text is separate
from binary UART1 or IICA0. Tool and bridge keep their existing ownership split;
the host forwards bytes and never interprets device metadata or Flash addresses.

| Device contract | INFO byte0 | Metadata protocol ID | Update transport | Link settings |
| --- | --- | --- | --- | --- |
| UART ISP | 1 | 0x55 | UART,0x04 | UART1,115200/8-N-1 |
| I2C ISP | 2 | 0x49 | I2C,0x02 | IICA0,7-bit0x20,100kHz |
| XMODEM-CRC128 | 3 | 0x58 | UART,0x04 | UART1,115200/8-N-1;128-byte CRC blocks |

The shared IS framing and256-byte ISP-NUL metadata remain in the device layer.
IRQ architecture is independent of serial framing. RL78 F1V1/F2V1/G6V1/G2V1
requires a516-byte minimum image and checked BR slots. RH850 R8V1 requires
0x1004bytes,entry/EBASE branches and all512 INTBP pointers. The PC validator
rejects unknown serial IRQ ABIs rather than skipping architecture validation.
The live client checks the architecture-specific minimum as well as the
installed metadata;transport selection never substitutes for this check.
I2C metadata words0x30/0x34 contain compiled bus Hz/7-bit address(default100000/0x20);its UART policy field0x70 is0.
UART/Xmodem link words0x30..0x3F are0;policy UART baud0x70 is compiled(default115200),not fixed by the Tool.
Live contract, transport, identity, geometry, IRQ ABI and minimum Boot are
checked independently of an offline BIN and before any Flash write.

I2C uses a STOP-delimited request of14..110bytes and a separate111-byte read:
byte0 is00BUSY orA5READY; remaining bytes hold the IS reply thenFF padding.
The main loop parses requests and performs Flash work. The IICA ISR only moves
bounded bytes/state. A published reply stays immutable through a complete
ready read, final master NACK and STOP; only then may ENTER/END reset the MCU.
Short reads retain the reply. An abandoned mailbox has a bounded2s lease.

Xmodem retains framed INFO/metadata/START/version gates, followed by'C', SOH,
block/complement,128data bytes and CRC16/XMODEM. A final partial block uses1A
padding. The receiver commits each block once and compares duplicate payloads
byte-for-byte; one EOT invokes the full-image CRC32/launch gate once. A missing
EOT ACK is an unknown result, never authorization to resend END or rewrite.
App/Transfer command24 provides an explicit CRC streaming sink without Flash.
Inter-byte expiry and CAN/CAN cancellation are distinct from framed ABORT.

All four RL78 serial device families reserve8KiB for Boot with a minimum512-byte
spare. App software vectors start at2000, metadata at2100 and code at2200.
The Xmodem main loop passes complete128-byte blocks to the same checked update
engine, without constructing a second framed WRITE/END transaction. Framed
WRITE/END are rejected in that Boot protocol. The Flash verification adapter
checks expected size, stored CRC trailer, actual CRC, metadata and every App
jump slot before requesting reset. The Tool still confirms the App separately.

The RL78 Xmodem Boot binding owns one fixed device/request/reply context;
receiver and framed-control input share RAM only while the other is inactive.
Small state fields precede large arrays, and RAM payload helpers use explicit
RAM pointers. Flash reads retain full integer-address/far access. Boot omits
unadvertised diagnostic accounting; App/Transfer retain their counters. None
of these internal RAM layouts is a serialized metadata or wire-format change.

The production client selects the transport adapter, while metadata parsing,
version policy and image validation remain common. Source/model checks,
target linker/package checks and physical acceptance are separate evidence.

The RH850 serial runtime separately owns framing,Flash-page accumulation and
post-reply actions. Its Xmodem completion checks IspBusReplyComplete before
reset;control-TX failure cancels streaming and clears pending reset. Action
consumes pending state once. UART's synchronous send and RIIC's full read/
NACK/STOP are separate implementations of that completion boundary. Actual
RuntimePoll compiled-role tests complement engine and HAL models;none of
these substitute for target electrical/startup/FCL acceptance. RH850 source/
SC/runtime details remain in its dedicated local engineering guides.

