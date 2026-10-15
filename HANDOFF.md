# Development Handoff

Last updated: 2026-10-15

## Authority

1. Current user request
2. `DEVELOPMENT_CHECKLIST.md`
3. `docs/HOST_LINK_PROTOCOL_V2.md`
4. `docs/ARCHITECTURE_V2.md`
5. `docs/F13_TRANSFER_PROTOCOL.md`
6. `docs/F13_BOOT_PROFILE.md`
7. `docs/F13_BOOT_UPDATE_FLOW_zh-TW.md`
8. `AGENTS.md`
9. `SELF_CHECK.md`

## Current status

### Evergreen web navigation and copy (2026-10-15)

User requested removing draft/unpublished wording and prominent release /
single-run metrics from public web copy. Both RL78 Bootloader and ISP Tool
now have canonical navigation links and function descriptions. Cleaned both
slide intros, ISP performance narrative/UI milestone prefixes, READMEs and
snapshot wording in canonical metadata/F24 IRQ/porting blocks; installation,
protocol/image/IRQ requirements and unverified boundaries remain intact.
Exact build/hash/performance evidence stays in first-delivery/technical records.

verify_local.ps1 checks product identity and linked evergreen navigation instead
of requiring a literal current Tool build. New test_public_web_guides.py covers
four public-copy/link/contract/future-version cases; initial PS5 no-BOM regex
failure corrected using ASCII Unicode escapes, all four PASS. No product or
firmware change/rebuild, version bump, COM or publishing. Final public/guide/
PDF/porting10, canonical4,197local refs/fences,7HTTP routes plus navigation/
both MD sources, and safe clean24 PASS. PDFs30/10/11/9 pages generated with
20/5/6/2 diagrams, all rendered/contact sheets and changed detailed pages
visually reviewed. Tool EXE SHA unchanged. Actual browser/DPI and public
deployment NOT RUN; temporary loopback HTTP server stopped after checks.
Detailed changed files/results: tmp/internal/web_evergreen/VERIFICATION.md.

### First engineering delivery / user v5 evidence (2026-10-15)

User approved first delivery and plans to upload Tool/F13 Boot/App. Freeze
Tool0082/F24 Boot1.2.0-App1.5.0/F13 Boot-App1.5.0; no product rebuild/version bump,
COM/reset/flash, Git or publishing action. docs/FIRST_DELIVERY_20261015_zh-TW.md
contains exact artifacts/SHA-256, paired installation/source dependencies,
upload privacy/library-license cautions and normal-path-only acceptance.

User182253 log was actually read in the preceding diagnostic turn: COM6 at
115200 identifies F24 App1.5.0 and all256 metadata bytes; F13 protocol5,
19244B/CA0DF55C,301 sequential COMMIT ACKs,10963ms including metadata/END,
1755B/s, matching END, no UART timeout/retry/UI overflow/CAN lost/error.
About7.2x versus old79s, but different19224/19244B and one v5 run, not strict
repeated A/B. Earlier182108 selects COM7 and receives no reply; no firmware
failure established. Original user logs/INI were no longer present in build
when preparing delivery; did not recreate or delete them. Retain original
user evidence separately; the release note is a summary, not raw capture.

New post-reset App UART/INFO, native DPI/ghosting, real power-loss/bus-off/
disconnect/IRQ and high-baud stress remain OPEN. I2C programming/device UART/
new bridge backends remain deferred. MD/local web and affected PDFs updated;
test-only Debug/Release rebuilds0warnings/errors and both actual-image suites
PASS. Six frozen hashes/sizes and independent HEX checksum/combined byte/CRC/
reserved checks PASS. Python discovery24PASS/1optionalSKIP/1setupERROR because
F13 maps were removed; full map/symbol/IRQ audit cannot rerun without rebuild,
so retain earlier successful-build evidence and matching hashes. Guide/PDF6,
canonical4/web196/clean24 PASS; PDFs30/10/11 rendered and visually reviewed.
Detailed changed-file and verification ledger: tmp/internal/first_delivery/VERIFICATION.md.

### CAN v5 block update0082 (2026-10-15)

User171150 log establishes old v4/0080 DATA about77s and operation79s for
19224B/CRC1DE10DCD at115200/500K (0079 about142/145s). Trace misses DATA lines;
do not infer wire count from it. New0082 warns/counts UI queue drops and evicts
log events before state/completion when possible. No claim of complete capture.

Replaced DATA03 with64-byte RAM blocks: BEGIN0B(offset24/CRC32), indexed raw
standard data IDs720..727 (8bytes,4-byte final tail), COMMIT0C and one ACK with
offset/64 and next address. INFO1A declares64/base ID. Metadata staging/seal,
policy/layout/whole-image CRC,8-KiB Boot/App2000 and fixed IRQ ABI are preserved.
F13 Boot/App1.5.0 is a paired one-time reflash, no v4 compatibility. Boot compact
logging keeps clock/CAN/error/update/reset but removes RAM dumps/entry/size text;
end1DED gives530 spare bytes, retains512-byte guard. No partition expansion.

F24 App1.5.0 adds generic Host Link22/bit12,12-byte header and compact records;
10 full frames use122/128 bytes. All records validated before TX. Nonblocking
TX-complete/gap/final-RX, including early ACK, uses existing abort/timeouts and
UART replay. Bridge contains no F13 semantics. Shared can_burst codec and
App host_link_can_process.inc run in production and tests. Existing F24
Boot1.2.0 MOT hash unchanged; use Misc115200 to update App and explicitly RUN.
UI/worker require bit12; PC validates live v5 block capability before updates.
Only TIMEOUT/BAD_SEQUENCE/CRC_ERROR permit one whole-block retry; replay checks
Flash and never rewrites. Flash/wrong ACK/disconnect fail with best-effort ABORT.

Clean CC-RL F24 App PASS0errors/2existing license warnings:42696B/CRC864E811E.
F13 Boot0errors/4existing toolchain warnings; App0errors/3license notices:
19244B/CRC CA0DF55C,version1.5.0. PC Debug/Release0082 build0warnings/errors;
both actual-image suites and real-source block/HAL fault tests PASS. Python29
tests PASS with1explicit optional real CC-RL A/B fixture-build skip. Artifact/
IRQ/ROM/RAM/CRC and source-boundary guards retain original safety checks.

MD, canonical sync blocks, both local web guides/navigation/screenshots updated.
Four PDFs30/10/11/9pages rendered and visually reviewed. Local web196 refs and
current0082, four sync checks and workspace clean audit24 PASS. No Git/publish.
Computer Use list_apps still fails native pipe/os error2: native startup/DPI/
screenshots NOT RUN. No real COM probing/flash/reset in this turn. v5 physical
speed, UART high-baud stress, bus-off/power-loss/IRQ acceptance still OPEN.
Install F24App and F13pair, benchmark same v5 BIN at500K/Host115200 first; then
trace-off/high-baud runs, save summary blocks/ms/Bps/CRC/errors and post-reset App.
Changed-file/evidence details: tmp/internal/can_block_v5/VERIFICATION.md.

### PC serial latency0080 (2026-10-15)

User0079 log164225 plus UART screenshot demonstrates one real F13 CAN update:
19224B/CRC1DE10DCD,4806 ordered4-byte DATA/ACKs in about142s (135B/s), full
operation about145s, END matches and reset enters V2 App. No UART retry/timeout
or CAN lost/TX-failure/bus-off. This is not a complete hardware fault matrix.

Only production code serial_port.cpp/.h changed: documented Win32 first-byte
mode returns buffered/arriving bytes without waiting for full512bytes/20ms.
Read idle waits use remaining caller deadline capped50ms, zero deadline drains
without blocking, unchanged settings cached, Sleep polling removed. Existing
write timeout/baud settle, parser/CRC/retries/cancel and all firmware/wire
contracts preserved. No FTDI registry/driver changes or batch protocol added.

New serial_port_io_tests.cpp compiles actual SerialPort with Win32 API model;
registered in test vcxproj/main. RED on old source, GREEN after fix. Covers
short/buffered/partial/max128byte/coalesced frames, bad-CRC parser recovery,
deadlines0/7/123ms, DWORD wrap, config/unplug failures, cancellation and4806
Host Link transactions. This is not a real COM-driver latency measurement.
Release and Debug one-click builds and final actual-image suites PASS,
0warnings/errors;0080 EXE delivered3091456bytes, SHA256
27357731B88A82B36DE67A67BF9FAFEB3506D1D8D2A114DE2AA7647D507F0F04.
INI and F13/F24 Boot/App artifact hashes unchanged from this turn's baseline.

MD/local web/performance gate/navigation synchronized; three PDFs30/10/11pages
regenerated, shared/multi all pages and changed F13 page1 visually checked.
Startup3/App-route2/artifact3/guide2/porting4, four canonical sync checks,
web196/current0080 and workspace clean audit24 PASS. No publication/Git actions.
Computer Use initialized but list_apps fails native pipe/os error2, so native
startup/DPI/screenshot acceptance NOT RUN. No real COM/reset/flash or hardware
speed test performed. Next: same BIN/115200/500K/same trace A/B, then separate
trace-off/460800/921600 runs, at least3 each; record timings/CRC/error/overflow.
Optional internal evidence: tmp/internal/serial_latency/VERIFICATION.md.

### App metadata reconnect / startup settings0079 (2026-10-15)

User log154213 proves Host Link/INFO50 succeeds in App1.4.0 with VALID_APP,
then READ_METADATA57 returns BAD_COMMAND. The real inner App handler already
supports57, but host_link_protocol.c omitted its outer switch case. Added only
that App-owned route and bumped app_image.json to1.4.1; no metadata fallback,
Boot/partition/IRQ/baud/wire change. Clean CC-RL App build0errors/2existing
license warnings:40944bytes,CRC9F32E2B8. Boot MTPJ/vector/MOT hashes unchanged.

The separate empty-path INI error5 follows synchronous startup EN_CHANGE before
InitializeSettingsFile. Start loading_ui_settings_ true; guard App-start edit,
SaveUiSettings and empty-path writes; capture real Windows error immediately.
Existing INI content/ACL unchanged. Release0079 delivered, Debug/Release builds
0warnings/errors and both full actual-F24/F13-image test suites PASS. New tests
cover actual App C handler through PC metadata readback and two fresh sessions,
four64-byte pages each, bounds/no writes; outer route and startup order have
source regression tests (2+3). This is not full hardware/UART/MFC execution.

Old App recovery: close COM, hold physical SW1/P32 LOW during reset/power-up,
release after Boot recovery starts. Tool0079/115200 Probe must show LIVE BOOT;
Misc explicitly reload new App BIN, Update HOST APP, then separate validated
RUN. Re-Probe and close/reopen Tool to verify App1.4.1. No Boot reflash needed.
No COM, flash or reset performed here; physical recovery/reconnect and native
GUI startup remain NOT RUN. Prior native pipe failure remains unverified.

MD/local web/PDF sync complete; four PDFs30/10/11/9pages regenerated, all
shared/multi pages and changed F13/porting page1 visually reviewed. Guide2,
porting4, artifact3, F24-layout4, sync4, web195/current0079, clean audit24 PASS.
App BIN SHA2566AC36E63077EB106950344564ECF2B30DCCCF98F1ED12688F799A3CD65EB5813;
EXE6B2DC25E2437766FBDED23D997E649C6F5337631F72EFF6996133F0175649716.
Details/changed files/reproduction: tmp/internal/app_readback_fix/VERIFICATION.md.

### Progress footer clipping0078 (2026-10-15)

User's Misc update crop shows the second line below "Host bridge app" cut off.
Confirmed the145px fixed default-wrapping CStatic cannot fit the operation
caption plus percentage. Actual9pt Segoe UI/GDI reproduces old text heights
30/40/50px at96/120/144dpi; reserved single-line100% widths are171/215/258px.
The footer row was only24px at normal text and labels started3px too low.

Changed main_frame.cpp/.h/main_frame_layout.cpp, ui/console_geometry.cpp/.h:
explicit single-line progress style, actual-font width with100% reserved,
bounded non-overlapping label/bar/elapsed/build rectangles without y+3 spill.
Only changed operation name re-layouts, not each progress packet. No worker,
image, protocol, firmware or update-safety change. Added GDI/geometry regression
cases to tests/console_bridge_tests.cpp (all8labels,72width/font combinations,
360percentage cases plus unsupported narrow widths and original-defect proof).

Release one-click build bumped/delivered0.1.0.0078; Debug one-click and final
test-project rebuilds/full actual-F24/F13-package suites PASS,0warnings/errors.
INI and four F13/F24 firmware hashes unchanged. No COM opened, buses driven,
flash written, actual update, or native GUI smoke performed. Computer Use
initializes but list_apps fails: native pipe unavailable/os error2. This blocks
actual desktop screenshots/100-125%DPI validation, not the GDI geometry tests.

Updated README/AGENTS/design, this handoff/checklists, shared/multi/F13 guide
current versions, ISP/Boot local web text/navigation/screenshot gate. PDFs
regenerated in docs: shared10/multi11/F1330pages; visually reviewed all shared/
multi pages and changed F13 page1. PDF generator default diagram count aligned
with the existing F13 source20 (was stale18); diagrams5/6/20 rendered PASS.
PDF2/porting4/artifact3/F24-layout4 tests, sync4/web195/current0078/clean24 PASS.
No web publishing or Git actions. Details/logs under tmp/internal/progress_footer.
Delivered EXE SHA256 F9A460FF6BC0EFF780C37927B40BD6FEE3EB37A450B5B73DF3A98EEED5716C82.

### F24 two-product consolidation (2026-10-15)

Only firmware/RL78_F24_Bridge_boot and RL78_F24_Bridge_app remain buildable
products. Shared board/drivers/SC moved to firmware/f24_board; portable I2C
staging moved to firmware/common. App now owns main/custom_func/inc_main/
misc_config/host_link_protocol/can_fd_module. Shared UART/timer depend only on
f24_runtime_hooks.h, implemented separately by Boot/App, not App headers.
Standalone identity fallback is removed. The retired monolithic MTPJ, IDE state,
version-bump script and outputs are recoverable under tmp/internal/retired_f24_monolithic,
not active dependencies. All213 original files are accounted for by hashes;
only four intended source/include boundary adaptations differ.

prepare_f24_bridge_projects.ps1 now loads the two owned MTPJs independently,
retains compiler/linker/library/hooks/JSON, reconciles SC source registrations,
updates no-vect/startup projections and shared SC/wizard paths. Old debugger
ABS references point to the correct owned product. Default all remains
intentional; use -Kind app for reference App-only SC preparation. Custom Apps
still use app-local prebuild/build -AppProject, never reference preparation.
After moving the workspace refresh SC output paths with prepare before opening
SC; after SC generation prepare again before building. GUI regeneration at the
new shared location remains NOT RUN. Layout changes must align owned linker
options and the shared layout, not assume prepare replaces custom options.

PASS: both clean CC-RL rebuilds0errors/2existing license warnings and package
guards; Boot MOT/App BIN bit-identical before/after (App40936/CRC6E5FF561).
4 new structure/path/idempotency/SC reconciliation tests plus5 real A/B App
fixtures PASS; App-only operations preserve Boot vector/MTPJ/MOT hashes.
Debug one-click/direct Release and full actual-image PC test suites PASS;
F13 artifacts/3IRQ tests unchanged. Guide PDF2/porting4/artifact-layout3 tests,
4 canonical sync checks,195web refs/version0077,11+9Mermaid and clean audit24
PASS. Shared10-page and porting9-page PDFs regenerated and visually inspected.
Tool EXE/INI and version0077 unchanged. No COM traffic, flashing, IDE/native
GUI, actual clean deletion or web publishing. Detailed changed files/commands/
hashes and limitations: tmp/internal/F24_TWO_PROJECTS.md.

Initial F24 flow: programmer installs Boot once, then Misc115200 may install
matching App BIN and explicit validated RUN. Alternatively initially program
Boot MOT plus packaged App flash MOT. Misc never updates Boot. Docs, both
local web managed sections and PDF guides now state this explicitly.

### Disposable internal artifacts (2026-10-15)

Moved13 historical verification/plan MD files and build diagnostics/caches/
previous tools to tmp/internal:168 entries /416 files, SHA256 checked before
and immediately after every move. No deletion. Build now keeps the current
EXE/INI and any user runtime logs only; current executable/settings unchanged.
PC project/test OutDir/IntDir, build/clean scripts, F24 wrapper log output and
app IRQ fixture evidence now target tmp/internal/build, recreated as needed.
README/docs index explains disposal; authoritative guides/specifications and
firmware source/packages remain permanent. Archived reports preserve original
historical paths, not live specifications. References below are optional old
evidence and may disappear when the user deletes tmp; no build requires them.
Detailed relocation manifest/results are in tmp/internal, not another docs report.

Verification PASS: Debug one-click + direct Release MSBuild and both test
project builds0warnings/0errors; both full PC suites with actual bridge/F13
images;5 real F24 A/B fixture tests; layout3/porting4/PDF2 tests; graceful-close
mocks; canonical4/web195refs and clean audit24. Formal EXE SHA F29C11EB...AD07
and INI SHA DC6B4115...8963 unchanged. Release wrapper was not used to avoid
an unrelated version bump/copy; version remains0077. No actual GUI startup,
clean deletion, board operation, firmware-source change or web publication.

### Firmware-only Boot/App porting entry (2026-10-15)

Added docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md and reviewed9-page PDF:
reuse-vs-custom flow, F13/F24 memory/IRQ/startup/RAM, CS+ hooks, per-app mapping,
initial installation versus app-only update, debugging and acceptance gates.
Explicitly distinguishes F13 AA/55 from F24 00/A5 owner markers, fixed F24
BOOT115200 from APP negotiated Host Link, and F13's still-reference-specific
packaging/checker names/paths. No generic arbitrary-MCU guarantee.
Corrected stale metadata/core/format/version/callback text in F13 IRQ/README/
CRC docs and F24 split READMEs. Canonical block sync + drift/read-only tests,
boot web full content, ISP web entry and local root navigation added; old
callback teaching is retained with incompatible-contract warnings.

PASS:4 new tests, existing2 PDF tests, F13 artifacts/3 IRQ-model tests,
F24 app generator CheckOnly, all4 canonical sync checks,195web references/
0077/7HTTP routes, Boot11+ISP9 Mermaid,9-page PDF visual/parser checks,
clean audit24. Tool0077 and F13/F24 delivered binary hashes unchanged.
No C/C++/ASM/MTPJ edits, rebuilds, COM traffic, flash, INI, commit/push/deploy.
Browser layout/GUI and hardware gates remain NOT RUN. Full files and
reproduction: tmp/internal/docs/RL78_PORTING_GUIDE_VERIFICATION.md.

### Host Link baud0077 (2026-10-15)

Implemented APP feature11/05 INFO/06 SET/07 CONFIRM for nominal115200/460800/
921600. Power-up remains115200; old-speed ACK drains before divisor change,
2s missing-confirm/TX-stall rollback; confirmed idle links do not auto-switch.
Only selected COM gets bounded rate recovery. Normal close/exit restores
bootstrap best-effort. Reset re-probes bootstrap/board ID; Misc ENTER uses
fixed BOOT descriptor rate. BOOT advertises no switch feature and its MOT
SHA256 is unchanged. Device UART baud is separate compile-time policy2 at112,
with build helper/parser/UI/gate prepared; no UART target tab/backend enabled.
CAN/BUR1 policy1 and F13 bytes/protocol remain unchanged; G16 still deferred.

PC0077 Release/Debug final0warnings/0errors, actual-package suites exit0;
F24 split app1.4.0 (40936/CRC6E5FF561), boot and monolithic1.0.0 build47
clean0errors/2existing license warnings. Real UART C state/ring tests and PC
failure matrix include lost confirmation/sequence collision. F13 existing
artifact/IRQ audit PASS end1DD1/CRC1DE10DCD. Clean audit24 PASS. No board
flashed, COM opened, INI edited or publication. Native pipe error2 persists
after retry/reset; physical UART baud/error/throughput and UI/DPI NOT RUN.
Full current evidence and exact board checks: tmp/internal/docs/HOST_LINK_BAUD_VERIFICATION.md.
Final follow-up also PASS:5 real A/B IRQ tests with unchanged boot, all3
canonical sync checks,194web refs/0077/7HTTP/9+9Mermaid, PDF30/10/11pages
(20/5/6diagrams),2 PDF tests and visual review of latest rendered pages.

### Common metadata policy0076 (2026-10-15)

Metadata256/core128/reserved128 with UPD1 role/update-transports/IRQ ABI/minboot
is implemented. Shared UI18 fields preserves CAN BIN identity on I2C/Misc and
highlights wrong routes; UI/worker/device validate before erase. PC0076,
F13 boot/app1.4.0 format6/protocol4 (RESET0A/64chunks08/SEAL09 before START),
F24 boot1.2.0/app1.3.0/BUR1contract3. Initial matching boot/app reflash required;
partitions/fixed IRQ/app-local workflows unchanged. G16 new boot/app explicitly
deferred; programming still blocked, generic I2C retained.

Final PC Release/Debug0warnings/0errors/full actual-package suites exit0.
F13 clean builds/artifact/3IRQ tests PASS: end1DD1/558spare, app19224/
CRC1DE10DCD. F24 clean boot/app0errors/2license warnings each, app39652/
CRC3D5555F8. Five real A/B generation/build tests plus both39660-byte fixture
PC full suites PASS; current boot hashes unchanged during app-only stages.
Real F13 C validation/CRC, staging/retry/reset/disconnect and two768-cut flash
models PASS; no physical power-loss claim. Clean audit24 PASS. MD/web canonical
blocks/194refs/version0076/7HTTP/8+8Mermaid PASS. Three active PDFs30/9/10pages
visually checked,2 PDF tests PASS. Native UI helper stillos error2 after
recovery; physical update/debug/IRQ/disconnect/max CRC latency/power-loss NOT RUN.
No flashing/COM/INI edits/publication. Exact changed files, hashes, commands
and next board checks: tmp/internal/docs/COMMON_METADATA_VERIFICATION.md.

### F24 app-local IRQ engineering workflow (2026-10-15)

Each F24 split app owns app_irq_handlers.json/app_image.json; pre-build
generates only its table/identity. App-only -AppProject build/package resolve
that MTPJ's stem/artifacts and never write boot. Reference SC prepare supports
explicit -Kind app, retains local JSON, no canonical active-app-IRQ discovery.
All64 linked branches and real ISR RETI/reserved BRK RETB are audited alongside
existing ROM/RAM/RFD/stack/CRC/metadata gates. Wrong MCU/split flag/physical
owner/unregistered/static/ordinary ISR/stale files fail before delivery.

Actual A/B CC-RL builds use aaa/bbb versus ccc/ddd and distinct maps/versions;
5 generation/negative/integration tests PASS. Both app-only stages retain
identical boot vector/MTPJ/MOT hashes. Separate boot regression clean build
also retains the same MOT. Reference boot/app0errors/2license warnings each;
reference app38848/CRC15D3F05B unchanged size/CRC. Test evidence in
tmp/internal/build/verify/f24_app_irq is diagnostic only, not production delivery.
A/B actual BINs also PASS the PC parser/mock uploader and full suites, exit0;
this is not a physical upload or IRQ runtime test.

PC0075, UART boot1.1.0, reference app1.2.0 and BUR1contract2 unchanged. One-click
Debug0warnings/0errors; Release/Debug full actual-package suites, unchanged
F13 artifacts, clean audit24 PASS. Two F24 web blocks and prior F13 blocks
match canonical MD;194refs/current version, both pages7charts render PASS.
Shared PDF7pages/3charts visually reviewed; PDF parser2tests PASS, other PDFs
unchanged. No GUI source change or native UI test this turn; previous pipe
failure and physical A/B IRQ/update/debug/power-loss acceptance remain open.
No board flashing/COM/reset workflow, INI hand edits or publication performed.
Files/reproduction/hash evidence: tmp/internal/docs/F24_APP_LOCAL_IRQ_VERIFICATION.md.

### Fixed complete F13 IRQ interface0075 (2026-10-15)

F13 now owns all61 normal vector positions in independent boot assembly.
Compact dispatcher preserves hardware PC20/PSW and restores AX/HL/ES; app
INTTRJ0 is a real RETI ISR, not an ordinary callback. App-local JSON mapping
and ftable version/build generate table/identity in pre-build. Unused normal
slots RETI; reserved physical BRK7E not forwarded. Initial matching format5
boot1.3.0/app1.3.0 install required, old4 rejected. Future compatible app-only
IRQ/version changes do not require boot rebuild/reflash; arbitrary standalone
apps still need the documented MCU/partition/RAM/startup/metadata/CAN contract.

Boot clean build0errors/4warnings (3license+record overflow); app0errors/
3license warnings. Boot end1DC7,568free; app19184/CRCFC1DDC75. ROM/IRQ/RAM/
package checker and3 IRQ tests PASS, including7808 actual-opcode stack cases
and temporary alternate app mapping/version with unchanged boot assembly.
PC0075 Release/Debug0warnings/0errors; both full actual-package suites PASS.
Clean audit24, exact two web canonical blocks/194refs/version0075, PDF2tests,
40-page all-page visual QA PASS; F13 guide27pages/18charts. Three PDFs in docs.

Native startup/UI NOT RUN: computer-use list_apps native pipe os error2 after
retry and kernel-reset/reinitialize. Physical initial install/IRQ/debug/CAN/
power-loss MANUAL NOT RUN. No MCU flashed, no board workflow initiated, no
INI hand edit or Git publication. F24 firmware unchanged. Exact files,
reproduction, size comparison and gates: tmp/internal/docs/F13_FIXED_IRQ_VERIFICATION.md.

Implementation is finished; next action is the matching initial install and
real-board acceptance, not further speculative firmware changes.

### Left-only TABs and full F13 boot decision0074 (2026-10-15)

CAN/I2C/Misc TAB strip now occupies only left INTERFACE SETTINGS. Persistent
center SHARED IMAGE / METADATA and right SHARED LOG headings/panes are outside
TAB bounds. Existing single BIN/field/log ownership, INI keys and update gates
unchanged. No MCU firmware source or options changed; no MCU rebuild needed.

F13 guide section2.2 is now canonical complete gate/ENTER/inactivity/error/reset
documentation with two diagrams. sync_f13_boot_decision.ps1 synchronizes both
web pages and CheckOnly verifies exact matching blocks. Invalid-app recovery
has no inactivity timer; START rejection, ABORT and CAN/FSL failure distinctions
are explicit. Target README/protocol stale version/format wording corrected.

Release/Debug one-click builds0warnings/0errors and both complete actual-package
suites PASS; F13 artifact audit unchanged PASS; PDF parser/output2tests, web194
refs/version0074, ISP5/Boot5 Mermaid and clean audit24 PASS. Three PDFs directly
in docs, final5/8/25pages and2/4/17charts visually reviewed. Canonical/versioned
EXE matches compiled Release SHA9B9B453E...24AC4ECA.

Native UI startup/DPI/TAB screenshot acceptance NOT RUN: computer-use native
pipe os error2 after retry/reset. HTTP preview launch blocked by policy before
execution; no browser PASS. No board operation or publication initiated.
Exact files/results/reproduction:
[tmp/internal/docs/LEFT_TABS_BOOT_DECISION_VERIFICATION.md](./tmp/internal/docs/LEFT_TABS_BOOT_DECISION_VERIFICATION.md).

### Host128 / metadata256 delivery0073 (2026-10-15)

User explicitly waived old96-host compatibility. Payload128/frame142 is now
shared across PC/F24; metadata region256 at app offset100h..1FFh retains core96
and160 reservedFF. Bridge contract2 stages/reads four64-byte pages; six-byte
BEGIN validates the complete region before erase. F13 format4/CAN protocol3,
boot/app1.2.0; F24 split boot1.1.0/app1.2.0. Initial matching boot/app install
is required; Misc cannot replace boot. Code offsets remain unchanged.

Final PC Release/Debug0warnings/0errors and full actual-package suites PASS.
F24 split/standalone clean builds PASS0errors/2license warnings each; boot ROM
end4A90, app38848B CRC15D3F05B. F13 boot PASS0errors/5warnings (including vector
and record-type warnings), app PASS0errors/3license warnings; artifacts PASS,
BIN19160B CRC58FFF3FD. Clean audit24, web194references/0073 and Mermaid renders
PASS. Three guide PDFs directly in docs regenerated/all35pages reviewed; user
closed the F13 PDF lock. Final root/versioned0073 EXE matches compiled Release
SHA A6A3F455...0568199D. Existing unrelated work and INI content not hand-edited.

The one-click build actually closed project tool PID10016 normally; no forced
termination. No explicit board programming/update/reset workflow initiated;
normal cleanup is not a guarantee of zero traffic. Native startup/DPI/TAB and
physical UART/CAN/flash/debug/power-loss acceptance NOT RUN. G16 programming
still BLOCKED. Web local only, screenshots pending, no commit/push/deploy.
Exact files, commands, results and next gates:
[tmp/internal/docs/HOST128_METADATA256_VERIFICATION.md](./tmp/internal/docs/HOST128_METADATA256_VERIFICATION.md).

### Mandatory MD/web sync and authorized build closure (2026-10-15)

Every functional change must synchronize affected MD/PDF and local ISP web
instructions/status/screenshots; update boot-loader/home navigation when
affected. User also authorizes automatically closing this project's tool for
rebuild/replacement without asking again. This does not authorize publication,
flashing, or closing another project's tool/IDE.

scripts/build_mfc.ps1 now uses close_project_tool.ps1: full executable path
under this build directory plus fixed/versioned basename, normal main-window
close,15second wait for worker/COM/log/INI cleanup. Removed name-only forced
termination. Rejection/timeout stops build without force; false opts out.
Process mocks PASS, Debug one-click check0warnings/0errors, clean audit24 PASS.
No real process was started/closed or board command sent in this follow-up.

Local ISP guide now matches0072 (no parent scroll, shared console, Misc HOST
APP UART/F24 split artifacts), with screenshots still pending. Boot-loader
chapter and homepage preparation row synchronized, preserving unrelated edits.
194 local references/version/fences PASS; ISP3/Boot3 charts rendered and new
Misc chart visually reviewed. HTTP preview command was blocked by execution
policy; no HTTP/browser/desktop PASS claimed. No commit/push/deploy.
Existing0072 PDF already describes these features and was not regenerated;
EXE/version/INI/firmware unchanged. See tmp/internal/docs/BUILD_WEB_SYNC_VERIFICATION.md.

### Shared single-screen console and HOST bridge UART update (2026-10-15)

0072 implements user choices1366x768 and F24 boot20KiB/app0x5000. Parent scroll
removed; interface pane left, single BIN/16-field metadata pool center, single
log right, shared progress bottom. Five live/offline cards and full hover;
Misc last changes destination to HOST BRIDGE APP over UART. Old hosts without
feature400 remain blocked; I2C/G16 programming remains blocked. No GUI action
or physical board reset/erase/write was performed.

New F24 Bridge_boot/Bridge_app projects share canonical SC sources. Central
layout, explicit4KiB stack/handoff RAM, far constants, generated IRQ projections,
RAM RFD execution, commit-last manifest/CRC and automatic dynamic BIN packaging.
Boot excludes BRK007E physical forwarding. App package37644B CRC88FDB8C6.
PC final Release/Debug0warnings/0errors and full suites PASS including actual
package through real C parser/engine with mock flash; F24 boot/app clean builds
PASS0errors/2 existing license warnings; standalone compatibility build45 PASS;
unchanged F13 artifacts PASS, clean audit24 PASS,2-Mermaid/5-page PDF reviewed.

User closed the running tool after the root delivery lock was reported. Final
build/FirmwareUpgradeTool.exe now matches compiled Release SHA150CEFDB...63C13;
versioned0072 fallback remains beside it. INI/product identity unchanged.
See tmp/internal/docs/CONSOLE_BRIDGE_VERIFICATION.md for exact files/commands/results/gaps.

Next exact step: actual1366/DPI/TAB startup/repaint check (native pipe os error2
still unavailable), then explicitly planned initial debugger installation and
physical F24 IRQ/BRK/debug/UART/update/power-loss gates. Do not claim hardware
PASS from the builds or publish existing web drafts automatically.

### TAB/scroll ghosting repair (2026-10-15)

User screenshot showed old labels/button borders overlapping current controls
after CAN/I2C switching and vertical scroll in the bright UI. Source confirmed
OnVScroll used Invalidate(FALSE), while LayoutControls moved/hid many controls
without a final erase/all-child redraw. Parent background is painted only by
OnEraseBkgnd and the frame uses WS_CLIPCHILDREN, so exposed old pixels need an
explicit background erase and child-tree invalidation.

Tool0.1.0.0071 adds central deferred RDW_INVALIDATE/ERASE/ALLCHILDREN/FRAME after
the final layout, with valid-parent HWND guard. A scrollbar clamp reruns layout
and returns before the intermediate repaint; OnVScroll no longer appends an
erase-free invalidation. Only src/main_frame_layout.cpp behavior changed;
existing roles/colors/INI/bus/protocol/CRC/safety are preserved. No firmware or
web slide change. Release/Debug x64 builds PASS0warnings/0errors, both full
suites PASS and clean audit PASS24. Evidence: tmp/internal/build/diagnostics/repaint_*.
See tmp/internal/docs/UI_REPAINT_FIX_VERIFICATION.md for actual verification and reproducer.

Visual regression/startup/100%/125% DPI remains NOT RUN: native pipe os error2
after retry/reset. The source repair is not a claimed observed ghost-free UI.
Next exact step is run0071, switch CAN/I2C repeatedly and drag/page/line scroll
with and without BIN, including clamp at bottom after TAB/resize; save screenshot.
Do not connect/probe/program a board just to verify paint behavior.

### Bright identity overview and local web preparation (2026-10-15)

Tool0.1.0.0070 adds five provenance-aware overview cards: host bridge,
device target, live boot, live installed app, offline loaded BIN. Bold values,
measured detail wrapping/card height, hover full text and brighter palette.
HostInfo and Transfer DeviceInfo are retained in immutable worker snapshots;
validity gates suppress stale live data. Transfer boot/app stay N/A; G16
reference constants stay amber; raw G16/no manifest is amber, not CRC error.
No firmware/wire/update authorization change; CRC remains cached.

Final Release/Debug x64 builds PASS0warnings/0errors; both full test suites
PASS, including overview stale/provenance/N/A states. Palette text/background
contrast6.89..9.47:1; clean audit PASS24. Logs: tmp/internal/build/diagnostics/overview_*.
New identity PDF3pages/one Mermaid; multi-bus PDF8pages/four Mermaid. Both
Poppler-rendered and all pages visually reviewed. Details and changed-file
inventory: tmp/internal/docs/UI_VISUAL_REFRESH_VERIFICATION.md.

Local web sibling slide_isp_tool_for_RL78 now contains source, operation guide,
screenshot checklist and two SHA256-identical user Transfer benchmark images.
Existing slide_boot_loader_for_RL78 adds CAN without removing UART/I2C.
released.github.io README adds only a preparation entry; preserves the existing
user removal of the obsolete date. No Git repo initialization/push/deployment.
194 local references and7HTTP routes PASS; ISP2/Boot3 Mermaid render PASS.

Exact stopping point: implementation/tmp/internal/build/tests/docs complete; actual new UI
capture/startup/100%/125% DPI and browser visual QA remain NOT RUN. Computer
Use native pipe os error2 persisted after retry/reset; IAB unavailable and
browser inventory empty. No screenshots fabricated, no board traffic/reflash.
Next: restore desktop surface, capture safe idle/offline/host/self-test states;
device Boot/App instructions/upgrade/fault evidence wait for device readiness.
G16 programming remains BLOCKED pending corrected slave; RA remains future.

### Directory naming follow-up (2026-10-14)

User selected simple_renesas_isp_tool as the final directory name, while the
FirmwareUpgradeTool product/project/EXE/INI identities remain unchanged.
Local folder rename succeeded after the user closed VS Code/terminals. The
active tool/F24 source now resides in simple_renesas_isp_tool. Build scripts
derive roots from their own paths; external F13 contracts remain unchanged.
No Git repository/remote exists here; no new Renesas repository is created.
Original EXE/INI backed up. Post-move tool0.1.0.0069 Release/Debug x64 builds
PASS0warnings/0errors and both full HostLinkProtocolTests suites PASS. F24
clean /br build44 PASS,Error0/Warning2 (existing CC-RL license),
PROGRAM5FFA/ROM0295/RAM0D67. F13 artifact checker PASS unchanged:
app1.1.1/19160 bytes/CRC32 5AAE684B. Clean-script audit PASS24 scripts.
Verification logs/backups are outside Git in the operator's Temp directory
codex-folder-rename-57412d53ee78419d8fe063cf616db64d. The renamed multi-bus
guide was regenerated (four Mermaid/eight pages), Poppler-rendered and all
pages visually reviewed. Startup/UI smoke remains NOT RUN: Computer Use
native pipe unavailable after retry/reset. No firmware flashing or bus traffic
was performed; G16 programming safety block and all hardware gates remain.
Earlier software build/test results below are preserved baseline evidence.

### Active multi-bus work - source integration and software checks completed

User initially confirmed FirmwareUpgradeTool/firmware_upgrade_tool (directory
now simple_renesas_isp_tool) and first I2C target
G16. After CS+ was closed, the F24 project moved into the new firmware directory.
All active source/build/doc paths now use the new root. The old root retains
only the locked original cane vball schematic PDF; a SHA256-identical copy is
already in the new firmware folder. No user data or original project was deleted.
Existing legacy EXE/INI are archived under tmp/internal/build/legacy_can_tool; the existing
INI was copied unchanged to FirmwareUpgradeTool.ini. There was no original PDB
at the stable delivery root. Application follows AGENTS: new INI only, no runtime
legacy-filename import. Historical mechanical rename records below are CAN
baseline evidence, not newly run hardware verification.

Implemented and compile/mock verified:
- CAN/I2C tabs and green/amber/blue/red/gray text status cards, independent BIN
  fields, minimum-width/vertical-scroll layout and shared log/progress/summary.
- Generic bridge acceptance, advertised port catalogs and selection; old CAN
  host fallback and I2C-only capability parsing. Initial F24 is CAN0/IICA0.
- Generic512-byte staged I2C transport, whole262-byte packet assembly,
  repeated-start, bounded timeout/abort and dedicated F24 IICA0 backend. Only
  preserved Config_IICA0_user.c include/IRQ areas were changed in generated code.
- G16 inquiry/signature/area discovery and raw BIN CRC16/XMODEM inspection,
  independent from CAN manifest/CRC32. UI distinguishes live reported labels/
  versions/geometry from absent BIN identity/address metadata. Changing current
  port/rate/address invalidates the display until re-probed.
- Bus/port/address/speed INI persistence with a startup loading guard; visible
  one-time INI write failure; generic product/log/summary naming. CRC values are
  cached at load, not recomputed by every UpdateUi.
- Worker CAN workflows/formatting and main-frame image presentation split into
  responsibility modules; active UI/worker source files are below1000 lines.

Actual evidence (2026-10-14):
- Final tool0.1.0.0068 Release/Debug x64 one-click builds PASS,0warnings/0errors.
  tmp/internal/build/diagnostics/multibus_release_build.log and multibus_debug_build.log.
  Stable Release EXE is build/FirmwareUpgradeTool.exe (~3MB); Debug remains
  isolated under tmp/internal/build/debug/x64 and cannot overwrite stable delivery.
- Full Release/Debug tests PASS: real bridge_i2c engine bounds/262-byte continuity/
  timeout; real F24 backend register/ISR mocks; PC catalog legacy/multi-port/
  I2C-only, staged RX512/TX262, byte-identical EXEC retry, cancellation/errors/
  abort; G16 signature/geometry/CRC16 and all previous CAN/F13 tests.
  multibus_release_tests.log and multibus_debug_tests.log save actual output.
- Initial integration build failed on a mock clock duplicate symbol and a moved
  VersionText helper; both fixed. Clean removes rename MSB8028 intermediates.
- F24 post-move clean /br build43 PASS,Error0/Warning2 (existing CC-RL license),
  PROGRAM5FFA/ROMDATA0295/RAMDATA0D67. No new hardware operation was performed.
- F13 artifact checker PASS unchanged: boot/FSL end1C85, app1.1.1,19160 bytes,
  CRC32 5AAE684B. F13 Transfer/boot/app target source was not edited this task.
- Workspace clean-script audit PASS24 scripts; clean only compiler intermediates.
- Four multi-bus Mermaid diagrams/eight-page zh-TW PDF generated and rendered
  using Poppler; all pages reviewed, overly tall G16 flow split into two.
  F13 guide regenerated16 diagrams/22 pages after product rename; render review
  completed. Both stable PDFs reside directly in docs, not output/pdf.

Known limitations and exact next point:
- Supplied G16 slave active received_handler has no READ15 or ID-auth30 case;
  inquiry bypasses real ID validation. Signature is placeholder RL78_G16_FPB,
  version constants are not authenticated app metadata. Code area1000..7FFF,
  CRC16 LE2 at7FFC (last4-byte slot), erase1024/write-read256.
- Slave32-bit address fields narrow to16bits; ERASE always covers the full app;
  boot entry lacks active CRC gating and SWITCH_APP may skip CRC after final
  write. Therefore no G16 erase/write/app-jump runner is exposed. Fix/approve
  a separate slave contract before completing programming/read-back/recovery.
  Reference source was inspected read-only, not edited or reflashed.
- Actual F24 Host Link dispatcher replay/sequence logic is source-reviewed and
  compile-checked, not directly executed in a dispatcher harness or hardware.
  Fake PC retry tests are not evidence of actual bridge replay behavior.
- Computer Use skill fresh import, retry and kernel reset all report native
  pipe unavailable (os error2). Startup/tab/100%/125% DPI/small-window visuals
  NOT RUN. Do not substitute direct PowerShell UI automation.
- Physical F24 I2C waveform/clock stretching/NACK/repeated-start/recovery,
  actual G16 discovery and CAN regression remain MANUAL; no COM devices in
  current verification environment. WDT remains excluded; RA backends deferred.
- Next: visual validation when desktop helper works; F24 reflash and explicit
  G16 discovery with analyzer, then separately agree corrected slave identity/
  image/flash/recovery contract. Preserve working CAN benchmark and F13 targets.

Changed-file groups and reasons:
- src/core/bridge_port_catalog.*,i2c_host_client.*,g16_i2c_* and Host Link client/
  codec: additive generic transports and strict separate G16 integrity contract.
- src/main_frame_bus/status/i2c_image/i2c_summary.*, status_card.*, frame layout/
  settings/image/workflow and worker_bus/worker_can_workflows/worker_format.*:
  shared UI, honest status, asynchronous ownership and responsibility split.
- F24 bridge_i2c.*,f24_i2c_backend.c,host_link_protocol.c,project and preserved
  IICA0 IRQ hook: target-independent staged transport and board-only registers.
- tests/bridge_i2c_tests.c,f24_i2c_backend_tests.c,f24_i2c_mock.h,multibus_tests.cpp,
  test entry/project: real-source and fake-transport regression coverage.
- Product/project/scripts/docs: rename, checked wire/layer contracts, reusable
  PDF generator and multi-bus zh-TW source. See SELF_CHECK for verification gaps.

The Phase 0 protocol/architecture baseline is written and Phase 1 builds are
recorded. F24 board I/O, bounded binary UART, the Host Link v2 control plane,
and the CAN safety foundation build successfully. All three active F13 projects
now build with Classical CAN configured for 500 kbit/s. F13 Transfer now uses a
deterministic non-destructive protocol instead of the original echo demo.

Completed documentation work:

- Defined Host Link Protocol v2 framing, commands, status values, CAN record,
  capabilities, exact-filter rules, retry/replay policy, and conformance tests.
- Defined the PC/host/target ownership boundary and target-profile architecture.
- Removed the obsolete Host Link v1 protocol document.
- Replaced stale product documentation so it no longer describes the copied
  Nuvoton design as the active project.
- Added the source-verified Traditional Chinese F13 boot/update Mermaid guide
  and its PDF under `docs`; both remain hardware-pending release evidence.

## Important implementation truth

Host Link v2 is integrated into the F13 Transfer diagnostic and metadata-driven
CAN Upgrade UI paths.

The current UI calls the diagnostic path `Custom Transfer`. Its v1 contract is
MCU-independent at discovery time: protocol/capabilities and identity syntax
are validated, but target `0xF13B` and part `R5F10BMG` are no longer hard-coded
acceptance requirements. `RL78_F13_CAN_Transfer` remains the first separately
flashed reference/transport-isolation target; its BIN is not a CAN Upgrade
image and must not be loaded through `Load Update Image`.

- PC solution/project/executable/resource/window/version/INI/log names now use
  `FirmwareUpgradeTool`; the copied legacy INI filename is not read or written.
- The PC worker exclusively owns serial, Host Link, generic CAN, and target
  client state. Host/target discovery and F13 Transfer tests do not run on or
  pump the UI thread.
- The copied Host Link v1 bridge/update modules and their Nuvoton-specific UI
  flow are removed from this project.
- CAN Upgrade v3 validates the variable image length, 96-byte format-3
  identity/layout/CAN manifest, and CRC32 before START. Discovery reads all
  non-destructive INFO metadata pages from app or boot and uses HELLO after boot
  state is known. ENTER is explicit, DATA byte-offset/address ACK handling is
  strict, and error/cancel after START attempts ABORT plus CAN_STOP before COM
  teardown.
- F24 implements bounded binary UART, the v2 parser/control commands, and the
  generic CAN TX/TX-wait-RX state machine with one active plus two pending
  transactions. It emits asynchronous RX/state/error events and implements
  ACK-before-reset `RESET_HOST`; the PC performs bounded same-port reconnect
  and identity probing. Physical PC/F24/F13 verification remains open.
- The F24 CS+ project, `.scfg`, I/O header, and active generated files use
  `R7F124FLJ3xFB`. A clean regenerated build passes. Historical FPJ5 records
  remain in inactive project metadata and still cause a CS+ device-change
  notice.
- F24 CAN callbacks use a bounded four-frame queue and do no formatted output.
  All remaining F24 `printf` calls and the `putchar` retarget are removed; the
  clean linker map contains no formatter symbol, so P15/P16 are binary-only.
- The F24 startup clock now uses the same-board-proven HOCO at 40 MHz instead
  of waiting for X1/PLL before `main`; CRXD0/CTXD0 routing is corrected from
  P73/P72 to the board's P11/P10. Physical reflash/probe verification is open.
- F13 Transfer, boot, and app active RS-CAN configurations now use 500 kbit/s;
  hardware bus verification is still open.
- F13 boot/app now reserve the `0xFE300-0xFE4FF` trace/self RAM window with
  `-OCDTR` and place normal RAM at `0xFE500` or above. The boot debugger runs to
  `_boot_main`; the app debugger retains `_main`. Physical E2 Lite halt and
  integrated boot-to-app debugging remain open.
- F13 Transfer protocol code and its PC codec are compiled and unit-tested, but
  physical filter, duplicate, timeout, error injection, and payload-integrity
  verification remain open.

Current builds are compile/link evidence only. They are not physical UART/CAN
or end-to-end Host Link v2 verification evidence.

## Recorded build baseline (2026-10-12)

- PC x64 Release: PASS, 0 errors and 0 warnings.
- F24 host clean rebuild: FAIL, undefined
  `_R_Config_INTC_INTP0_Start`; 1 error and 3 warnings. CS+ also requests I/O
  header regeneration after a device change.
- F13 Transfer clean rebuild: PASS, 2 warnings; program `0x41BF`, ROM data
  `0x055B`, RAM data `0x00B2`.
- F13 boot clean rebuild: PASS, 5 warnings; program `0x19E3`, ROM data `0x0362`,
  RAM data `0x092D`.
- F13 app clean rebuild and update-image generation: PASS, 3 warnings; program
  `0x3DA5`, ROM data `0x0497`, RAM data `0x00A2`; generated image size 122,880
  bytes and CRC32 `0xC711A197`.
- Firmware warnings include the expired/missing paid CC-RL V1 license. F13
  projects were loaded with CC-RL V1.15.01 instead of their saved V1.13.00.
- CS+ can return process exit code zero when the build summary says FAIL; check
  the build summary in automation.

Post-500-kbit/s F13 result:

- F13 Transfer clean rebuild after deterministic protocol integration: PASS, 2
  warnings; program `0x4875`, ROM data `0x0463`, RAM data `0x00CC`.
- F13 boot clean rebuild: PASS, 5 warnings; program `0x19E1`, ROM data `0x0362`,
  RAM data `0x092D`.
- F13 app clean rebuild and update-image generation: PASS, 3 warnings; program
  `0x3DA3`, ROM data `0x0497`, RAM data `0x00A2`; generated image size 122,880
  bytes and CRC32 `0xDA8DB250`.
- All active `RSCANconf` XML/header pairs use 500 kbit/s. The unused Transfer
  `RSCANconf_default` reference folder remains unchanged and is not in the CS+
  project build.

Post-fix F24 result:

- Schematic page 2 confirms fitted MCU `R7F124FLJ3AFB_C`.
- `iodefine.h` was regenerated from `DR7F124FLJ.DVF` V1.10.
- INTP7/INTP8 now drive separate ISR flags; 20-ms debounce and diagnostics run
  in the main loop.
- P80/P81/P82 are active-high LED outputs with a non-blocking idle, connected,
  transfer-start, transfer-active, and error state machine.
- CAN STB P12 is driven low for normal mode. Unused IICA0 is disabled and
  P62/P63 are released as inputs after generated initialization.
- F24 clean rebuild: PASS, 0 errors and 2 license warnings; program `0x59A8`,
  ROM data `0x09BC`, RAM data `0x00FA`.

F24 startup-clock/CAN-routing correction (2026-10-14):

- A physical report showed CS+ did not reach `main` and PC host probe timed
  out, while the same board ran the HOCO-based SelfTest project.
- The active host `.scfg` and generated configuration now use HOCO 40 MHz with
  X1/subclock/PLL stopped during reset-to-main startup.
- CRXD0/CTXD0 are corrected from the P73/P72 alternate route to P11/P10
  (`PIOR46=0`), matching the board schematic.
- Clean rebuild: PASS, firmware `1.0.0.17`, 0 errors and 2 license warnings;
  program `0x4C70`, ROM data `0x0275`, RAM data `0x0919`.
- Reflash, CS+ main-break, physical Host Link probe, and CAN waveform checks
  remain open and must not be inferred from this build result.

F24 UART transport slice:

- Removed the single-character demo commands, including UART-triggered reset.
- Added bounded 256-byte RX and TX rings with saturating error/overflow
  counters. RX/error/TX-complete work in ISR context is limited to queue/state
  updates; TX start runs in the main loop.
- This interim slice discarded `putchar` output so formatted diagnostics could
  not corrupt Host Link. The latest cleanup supersedes it by removing every
  F24 `printf` call and the `putchar` retarget from the linked firmware.
- Clean rebuild after the UART change: PASS, 0 errors and 2 license warnings;
  program `0x5BAE`, ROM data `0x0998`, RAM data `0x030E`.

PC Host Link v2 codec slice:

- Added a separate version-2 codec with the 96-byte hard limit, v2 command and
  status values, strict type/status checks, header-first length validation, and
  a bounded 512-byte stream buffer.
- Added a standalone Visual C++ console test project to the solution. It covers
  the CRC known vector, empty/96-byte round trips, 97-byte rejection, corrupt
  headers/payloads, split/concatenated frames, noise/overlapping SOF recovery,
  and input-overflow accounting.
- x64 Release solution build: PASS, 0 warnings and 0 errors.
- Standalone test-project build: PASS, 0 warnings and 0 errors.
- Added a transport-independent v2 client/session over the codec. It allocates
  non-zero sequences, matches sequence plus command, retries the byte-identical
  encoded request, queues asynchronous events with a fixed limit, and parses
  and validates host information/capabilities.
- `SerialPort` implements the v2 byte-transport interface without changing its
  existing call surface.
- Fake-transport tests cover host probe and PING, payload parsing, interleaved
  event and wrong-sequence response handling, identical retry, and no retry on
  a host NACK.
- x64 Release solution rebuild: PASS with no compiler warnings.
- Test execution after F13 Transfer profile integration: PASS, `Host Link v2,
  CAN client, and F13 Transfer profile tests passed`.
- The later worker/UI slice connected this client to active UI actions and
  removed the copied Host Link v1 bridge/update modules.

PC generic CAN host client slice:

- Added target-independent configuration/filter/start/stop/TX/TX-wait-RX APIs
  over the v2 session client.
- Added strict PC-side validation for IDs, masks, enabled zero masks, frame
  flags, DLC/data length, timestamps, receive timeout, and retry count.
- TX-wait-RX calculates a Host Link response budget covering every requested
  CAN attempt plus margin; a lost completed response can still use one
  byte-identical Host Link retry.
- Fake-transport tests verify exact wire payloads, state/result decoding,
  received-frame decoding, and rejection before UART write.
- x64 Release solution rebuild and test execution: PASS.
- The generic CAN client is connected to both active MFC target profiles.

F24 Host Link v2 control-plane slice:

- Added a fixed-buffer v2 parser/encoder with a 96-byte payload limit, header
  CRC validation before length use, payload CRC validation, overlapping-SOF
  resynchronization, and a 64-byte main-loop processing budget.
- Added `PING`, `GET_HOST_INFO`, `GET_CAPABILITIES`, `GET_STATS`, and
  `CLEAR_STATS`. Other commands return `BAD_COMMAND` until their state machines
  are implemented.
- Added a last-response replay cache. Identical sequence/command/payload-CRC
  retries replay without dispatch; reuse of the sequence with different input
  returns `SEQUENCE_ERROR` without replacing the original cache.
- Development identity is deliberately `0.0.0`, build 0, board ID `0x0101`.
  Capabilities advertise confirmed Classical CAN, CAN FD, standard ID,
  extended ID, generic TX-wait-RX, nominal 500K, and data 2M support.
- Clean rebuild: PASS, 0 errors and 2 license warnings; program `0x63AB`, ROM
  data `0x0998`, RAM data `0x049E`.
- No physical UART request/response test has been run yet.

F24 CAN safety-foundation slice:

- Replaced both accept-all rules with one standard-data exact rule for F13 Boot
  response ID `0x701`, mask `0x7FF`.
- CAN initialization now leaves communication stopped. The future `CAN_START`
  command must explicitly start it.
- Replaced the unbounded communication-start/status loops with a 60,000-poll
  limit and an explicit error return.
- Replaced ISR use of the shared demo `rx_frame` with a four-slot RX queue. A
  full queue drains the hardware buffer into a discard slot and increments the
  saturating lost-frame counter.
- Removed all formatted output and frame dumping from CAN callbacks. Callbacks
  now only queue a frame or update bounded TX/RX/error counters.
- CAN RX/TX/lost/failure/warning/passive/bus-off/recovery counters feed the v2
  `GET_STATS` response.
- Clean rebuild: PASS, 0 errors and 2 license warnings; program `0x640E`, ROM
  data `0x0654`, RAM data `0x063E`.

F24 CAN configuration/control slice:

- Added Host Link `SET_CAN_CONFIG`, `SET_CAN_FILTER`, `CAN_START`, and
  idempotent `CAN_STOP` dispatch.
- Only Classical 500K and CAN FD 500K/2M presets are accepted. Configuration
  and filter changes return `BAD_STATE` while CAN is running.
- The single filter validates index, enable, IDE, frame type, ID width, mask
  width, and rejects a zero mask when enabled. START resets the hardware rule
  table and applies only the pending rule.
- P12/STB is high while stopped, goes low before START, and returns high on STOP
  or a failed start.
- The response replay entry is now committed before attempting UART enqueue, so
  a UART-full retry cannot repeat a completed side effect with a new result.
- Clean rebuild: PASS, 0 errors and 2 license warnings; program `0x6913`, ROM
  data `0x0667`, RAM data `0x069D`.
- No physical CAN start/filter/timing test has been run yet.

F24 generic CAN transaction slice:

- Added validated conversion between Host Link CAN records and driver TX/RX
  frames for Classical and CAN FD data, standard/extended IDs, RTR, BRS, DLC,
  payload, timestamp, and controller result.
- Added non-blocking `CAN_TX` completion and `CAN_TX_WAIT_RX` state machines.
  TX, abort, and response timeouts are bounded; matching uses ID/mask, selected
  flags, and optional DLC; retry count is limited to 0..3.
- Existing polling-mode TX completion is supported without enabling a generated
  interrupt. A callback event path is also retained for a later Smart
  Configurator setting change.
- Byte-identical in-flight retries do not retransmit CAN; a different sequence
  receives `BUSY`, and changed content on the active sequence receives
  `SEQUENCE_ERROR`.
- Clean rebuild after active-duplicate handling: PASS, 0 errors and 2 license
  warnings; program `0x75E2`, ROM
  data `0x0675`, RAM data `0x0812`.
- State-machine behavior still requires physical UART/CAN trace verification.

F13 Transfer deterministic-protocol slice:

- Assigned standard-data request/response IDs `0x710` / `0x711`; the target
  hardware rule uses exact mask `0x7FF` and the active RS-CAN timing is now
  genuinely 500 kbit/s (prescaler 2).
- Replaced arbitrary same-ID echo with fixed eight-byte protocol version 1,
  non-zero sequence, three command bytes, and CRC-8/ATM.
- Implemented non-destructive `DEVICE_INFO`, `PING`, `ECHO`, `PATTERN_TEST`,
  `GET_STATS`, and `CLEAR_STATS`. Identity pages report logical target `0xF13B`
  and exact part `R5F10BMG`.
- Added byte-identical duplicate replay, same-sequence changed-content
  rejection, bounded initialization, and non-blocking TX timeout/abort state.
  Warning, error-passive, and bus-off ISRs only update saturating counters.
- Added the matching PC target codec/client with strict response validation,
  identity assembly, non-zero target sequences, and discovery/PING/ECHO/
  pattern/statistics workflows over generic `CAN_TX_WAIT_RX`. Fake Host Link
  tests cover exact match construction and every target command.
- F13 clean rebuild: PASS, 0 errors and 2 license warnings; program `0x4875`,
  ROM data `0x0463`, RAM data `0x00CC`.
- Physical F24/F13 transfer, filter, duplicate, timeout, overflow, and error
  injection tests have not been run.

PC product rename slice:

- Renamed the solution, project, filters, resource file, executable target,
  root namespace, application class, window title, version symbols, INI file,
  log prefix, and build-script process/path references to `FirmwareUpgradeTool`.
- Preserved the existing solution/project GUIDs and native C++17 static-MFC
  deployment settings.
- Clean x64 Release solution build: PASS, 0 warnings and 0 errors; output
  `build\FirmwareUpgradeTool.exe`. The current verified version is `0.1.0.0042`.

PC worker-session and F13 Transfer UI slice:

- Added a single worker owner for the serial handle, Host Link v2 client,
  generic CAN client, and F13 Transfer client. The UI only submits commands and
  consumes posted immutable log/state/progress events.
- `Open + Probe Host` requires F24 board ID `0x0101`, compatible Host
  Link v2, and valid capabilities before CAN/target controls are enabled.
- `Connect Target` configures Classical CAN 500 kbit/s, applies the exact
  standard-data `0x711 / 0x7FF` filter, starts CAN, and assembles all F13
  Transfer identity pages.
- `Transfer Test` runs non-destructive PING, ECHO, PATTERN_TEST, and all
  GET_STATS requests on the worker thread.
- Host Link cancellation now has a tested specific result and checks at most
  every 50 ms while waiting for serial response data. The worker, not the UI,
  stops CAN and closes COM.
- Removed nested UI message pumping and deleted the copied Host Link v1
  bridge/update source modules from the project.
- The main window starts maximized and displays validated host/target identity.
- x64 Release incremental build: PASS, 0 warnings and 0 errors. Fake-transport
  tests: PASS, including the new cancellation case. Physical UART/CAN tests:
  NOT RUN.

PC F13 Boot profile and update-worker slice:

- Added strict raw-image validation for the fixed 122,880-byte image, jump
  entry, 16-byte manifest, target `0xF13B`, initialized version, image size,
  and reflected CRC32 stored in the final four bytes.
- Added HELLO, explicit ENTER, START, DATA, END, and ABORT framing/status
  handling over the generic CAN client. Responses require exact standard ID,
  frame flags, DLC, command, successful status, sequence, and address/CRC.
- The UI exposes separate `Discover Target` and `Enter Bootloader` actions.
  Discovery reads paged INFO from app or boot and never resets a running
  application; boot state is additionally validated with HELLO.
- `Transfer Test` and the fixed-label `Start Firmware Update` are separate
  controls. The update control has an adjacent dynamic list of missing
  prerequisites and cannot be enabled by merely loading a non-empty file.
- The worker uses 500-ms normal, 3000-ms erase-boundary, and 5000-ms END
  target timeouts with one target-level retry. DATA advances only after the
  matching ACK reports the exact next address.
- The complete image is revalidated before START. Update confirmation displays
  file, target, version, size, and the application flash range.
- Error/cancel after START runs a bounded cleanup section that attempts ABORT
  and CAN_STOP before a queued disconnect closes COM. Successful END requires
  the target CRC32 and is followed by CAN_STOP while the F13 resets to app.
- Added `docs/F13_BOOT_PROFILE.md` and fake-transport tests for image rejection,
  exact frames/matches, identity, ACK validation, timeout selection, and
  invalid-image zero-traffic behavior.
- Clean x64 Release build `0.1.0.0032`: PASS, 0 warnings and 0 errors. Unit
  tests: PASS (`Host Link v2, CAN client, F13 Boot, and F13 Transfer tests
  passed`). GUI launch smoke test: PASS with window title `Firmware Upgrade Tool
  v0.1.0.0032`. The generated app image at that slice also passed an independent
  manifest/CRC check: 122,880 bytes, target `0xF13B`, version `0x00010000`,
  calculated/stored CRC32 `0xDA8DB250`. Physical PC/F24/F13 update and
  fault-injection tests: NOT RUN.

Profile-specific image validation and UI split slice (2026-10-14):

- Split settings and workflow handlers out of `main_frame.cpp`; the main file
  is now below the workspace 1000-line limit and each handler has one definition.
- Added `core/firmware_update_profile` as the UI-facing validator catalog. F13
  retains its 122,880-byte rule, but the generic UI no longer embeds that size
  or its flash end address. Future RL78/RA profiles provide their own rules.
- File load/profile change caches validation; `UpdateUi()` reads the cache and
  does not recompute CRC. The destructive click performs a final validation.
- PC x64 Release `0.1.0.0045` and Debug: PASS, 0 warnings / 0 errors. Release
  and Debug unit/mock tests plus control-state startup smoke pass. Physical
  PC/F24/F13 testing remains NOT RUN.

F13 identity, robustness, diagnostics, and automation slice (2026-10-13):

- Added compatible INFO `0x07` pages to F13 app and boot, with running state,
  target/protocol, firmware version, and build. The app includes the boot
  project's authoritative identity configuration.
- Bounded F13 CAN init/mode/TX polls and update inactivity behavior. An invalid
  app is never entered after CAN initialization failure; boot logs and resets
  to retry recovery.
- Added PC CAN mode/data/filter controls, validation, persistent settings,
  default-off detailed UART/CAN trace, decoded target fields, UTF-8 logging,
  and exportable session summaries.
- The PC now parses the F24 `GET_STATS` 56-byte response, logs the CAN health
  counters, retains the final disconnect snapshot, and exports all fourteen
  UART/CAN/Host Link counters in the session summary.
- Added a reusable full-update runner plus mock normal/duplicate/timeout/
  corrupt/out-of-order/disconnect/cancel/CRC/ABORT fault injection.
- Added `scripts/verify_f13_artifacts.ps1` for boot growth boundary, app
  address/layout, identity, manifest/CRC, and shared-vector checks.
- Added F24 bring-up, diagnostics, target-profile, and generic future-host
  backend guides. RA2L1/RA6 firmware itself remains deferred until F24/F13
  hardware verification passes.
- F24 active Smart Configurator/generated code uses `R7F124FLJ3xFB`; regenerated
  clean build PASS, Error 0 / Warning 2, program `0x75E9`, ROM data `0x0675`,
  RAM data `0x0812`. The historical device-change notice remains.
- F13 boot clean build PASS, Error 0 / Warning 5, program `0x1B95`, ROM data
  `0x0390`, RAM data `0x092E`. F13 app clean build PASS, Error 0 / Warning 3,
  program `0x3FD4`, ROM data `0x04B3`, RAM data `0x00A2`.
- PC clean x64 Release `0.1.0.0038` PASS, 0 warnings / 0 errors; unit/mock tests,
  artifact checker, generated-ISR static check, and GUI title smoke PASS.
  Current update image: 122,880 bytes, target `0xF13B`, version `0x00010000`,
  stored/calculated CRC32 `0x0A38E6B9`.
- Physical PC/F24/F13 communication, update, LED/filter/timing, power-loss,
  shared-vector runtime, and FSL failure/recovery verification remain NOT RUN.

F24 reset/event/queue and UART-cleanup slice (2026-10-13):

- Added F24 firmware semantic version `1.0.0` with an automatically incremented
  build number returned by `GET_HOST_INFO`; the latest verified build is 5.
- Added one active plus two pending generic CAN transactions, duplicate-safe
  queue handling, and advertised maximum outstanding capacity 3. F13 update
  DATA remains serialized by its target ACK.
- Connected asynchronous CAN RX/state/error events through the PC worker. Raw
  error sources, controller state, bus-off/recovery, and cumulative arbitration
  loss are decoded into the PC log and session state.
- Added explicit Reset Host UI/workflow. F24 sends the ACK, waits for UART idle,
  and resets; PC closes the old handle and retries/revalidates the same COM and
  baud for a bounded interval.
- Removed every F24 `printf` call plus the `putchar` retarget. Clean F24 build
  `1.0.0.5`: PASS, Error 0 / Warning 2, program `0x4CF2`, ROM data `0x0275`,
  RAM data `0x0919`; source/map formatter checks PASS.
- Defined the product WDT policy from the project option bytes: F24 `0xEF` and
  F13 boot/app `0xE9` have `WDTON=0`. Neither MCU design uses WDT. Recovery uses
  bounded software timeouts, explicit reset, boot retention, and image
  validation.
- Added `docs/F13_BOOT_UPDATE_FLOW_zh-TW.md`, centered on F13 boot decision,
  app INFO/ENTER, shared ISR dispatch, flash/CRC update and recovery. Secondary
  diagrams cover PC/F24 Host Link and the non-destructive F13 transfer test.
- Removed the inherited P13.7 boot-entry check because it was not in the device
  requirements. Explicit ENTER/RAM marker and invalid-image retention remain;
  the app's independent P13.7/INTP0 behavior is unchanged. Added a dedicated
  Tool/F24/F13 `Discover Target` sequence to the Mermaid guide.
- PC x64 Release `0.1.0.0042`: PASS, 0 warnings / 0 errors. Unit/mock tests,
  including one-shot reset, background event routing, bounded event overflow,
  idle timeout, and simulated target FLASH_ERROR/FSL cleanup: PASS. F13
  artifact checker: PASS, CRC32 `0x0A38E6B9`.

F13 variable-length update and flow-document slice (2026-10-14):

- Image format is now 2. Application start remains `0x2000`; `0x1E000` is the
  capacity, not a required BIN length. Manifest size is the exact aligned BIN
  length and CRC32 occupies its final four bytes.
- F13 app post-build derives the CRC address from loaded-section end symbols in
  the linker map, patches the manifest placeholder, and emits
  `flash2000_variable_CRC.hex` plus the variable update BIN. It does not infer
  size from padded HEX range or the last non-`0xFF` byte.
- Tool START, DATA/progress count, and END sequence use actual image length.
  Unit/mock coverage accepts 32/60/80/120 KiB and rejects undersized,
  oversized, unaligned, manifest-size-mismatched, and bad-CRC images.
- The F13-centered Traditional Chinese Mermaid guide now has 12 diagrams and
  explicitly shows dynamic image generation/update plus the secondary
  Tool/F24 opaque bridge/discovery flows.
- Final software evidence: F13 boot PASS, Error 0 / Warning 5, PROGRAM
  `0x1BE1`, boot/FSL end `0x1DBB`, growth margin `0x244`; F13 app PASS,
  Error 0 / Warning 3, PROGRAM `0x3FD6`. The real app artifact is `0x42E8`
  bytes, CRC address `0x62E4`, version `0x00010100`, CRC32 `0xFB79333A`.
  Artifact checker PASS. PC `0.1.0.0047` Release/Debug PASS with 0 warnings /
  0 errors; Release/Debug unit/mock tests, Release startup smoke, UTF-8/link/
  Mermaid static checks, and 24-script workspace audit PASS.
- Bytes in complete flash blocks after a shorter declared image are outside
  its manifest and CRC and are not entered by boot handoff. Physical testing
  must include replacing a larger valid image with smaller 32/60/80-KiB
  images, plus power loss, shared ISR, and FSL fault injection.

Metadata-driven CAN Upgrade v3 and UI slice (2026-10-14):

- Replaced the MCU-name/120-KiB update profile with a 96-byte format-3 manifest
  and INFO pages `0x00..0x17`. Image and device now declare MCU family/part,
  protocol/format, app and boot version/build, installed image size, app
  start/capacity, flash/boot/erase sizes, minimum image, CAN modes/rates, ID
  format, and request/response IDs.
- The UI immediately shows BIN size, calculated/stored CRC and match status,
  app identity, layout and CAN metadata before any COM/CAN operation. Read-only
  discovery adds live boot/app/device metadata and programming requires an
  exact compatibility match.
- `App Start Override` is persisted in the INI. `Auto` reads metadata; an
  explicit value must match rather than relocating the image. DATA now carries
  a 24-bit byte offset and automated coverage includes alternate layouts and an
  image larger than 256 KiB.
- UI layout now measures button captions, uses wider/looser control spacing,
  labels the selection as `Workflow`, and splits long image/device metadata
  across visible lines. PC `0.1.0.0048` Release/Debug builds PASS with 0
  warnings/errors; both unit/mock executables and both startup smokes PASS.
  100% DPI captures at 1550x830 and 1188x675 show no caption/metadata overlap.
  The F13 artifact checker PASSes with boot/FSL end `0x1977`, margin `0x688`,
  image `0x444C`, CRC address `0x6448`, and CRC32 `0xC05CB0B9`. UTF-8/local
  links/13 Mermaid blocks and the 24-script clean audit PASS. 125% DPI,
  Mermaid rendering/PDF, and every physical-hardware item remain NOT RUN.

Variable-image UI and PDF verification slice (2026-10-14):

- PC tool `0.1.0.0050` now presents BIN size, calculated CRC32, stored CRC32,
  CRC result, metadata status, MCU, app version, build, target,
  protocol/format, app range, image/capacity, flash/boot, minimum/erase,
  CAN/rates, and CAN IDs as separate label/value fields. An unloaded clean
  launch displays `N/A` for every value.
- `App Start Override = Auto` uses the valid BIN manifest application start;
  after discovery it can show the device-reported start, but programming still
  requires an exact image/device match. A numeric override is a safety
  assertion and never relocates a BIN.
- The F13 app post-build remains map-derived and variable length: there is no
  60/80/110/120-KiB project-size define. The manifest records the exact aligned
  BIN length, and CRC32 is placed in its final four bytes. Current verified
  artifact: image `0x444C` bytes, CRC address `0x6448`, CRC32 `0xC05CB0B9`.
- Verification PASS: Release/Debug x64 builds (0 warnings/errors),
  Release/Debug unit/mock tests, F13 artifact checker, 24-script clean audit,
  and actual 120-DPI loaded/unloaded UI captures. The deliverable executable is
  the Release build.
- The Traditional Chinese flow guide contains 16 Mermaid diagrams. Mermaid CLI
  render and PDF generation PASS. The generated PDF has 21 pages; every page
  was rendered and visually inspected, and embedded Traditional Chinese and
  monospace fonts were confirmed. Tool workflow was split into discovery and
  programming/recovery diagrams to keep it readable.
- Remaining release gates are physical UART/CAN/filter/LED/button/bus-off,
  complete update/power-cycle/power-loss, runtime shared ISR, FSL fault
  injection, timing/queue stress, and captured normal/error logs and traces.

F13 E2 Lite debug-start correction slice (2026-10-14):

- Compared F13 boot/app project settings, startup, option bytes, clocks, linker
  maps, and symbols with the known-good `RL78_F13_SelfTest` project.
- Corrected all boot debugger Run-to-main settings from nonexistent `_main` to
  the startup's actual `_boot_main` symbol. The app correctly remains `_main`.
- Enabled linker `-OCDTR` in both projects and moved normal `.dataR/.bss` from
  trace/self RAM address `0xFE300` to `0xFE500`. The rebuilt boot stack ends at
  `0xFEE19`, below the shared boot/app marker at `0xFF700`.
- Documented standalone app debugging versus integrated boot-to-app debugging;
  standalone ABS download temporarily owns `0x0000`, while integrated testing
  must retain the boot vector and load app symbols without the app vector
  object.
- Regenerated the Traditional Chinese guide in `docs`: 16 Mermaid diagrams and
  19 PDF pages. All pages were rendered and visually inspected; the added E2
  Lite section is legible, fonts are embedded, no page is blank, and the PDF
  title metadata no longer contains mojibake.
- Clean CS+ rebuilds PASS. Boot: Error 0 / Warning 5, PROGRAM `0x1758`, ROM
  data `0x03D4`, RAM data `0x092E`. App: Error 0 / Warning 3, PROGRAM `0x418B`,
  ROM data `0x04B3`, RAM data `0x00A2`; update BIN `0x444C`, CRC address
  `0x6448`, CRC32 `0x6F3E6CEF`. The F13 artifact checker PASSes all layout,
  identity, CRC, and vector checks.
- Remaining verification is physical: confirm E2 Lite halts at boot `boot_main`
  and standalone app `main`, then perform integrated reset-through-boot app and
  shared-ISR runtime testing.

Custom Transfer dual-ended trace slice (2026-10-14):

- The F13 reference target now prints configurable `TRACE CT RX/TX` lines from
  main-loop protocol processing only. Each line includes ID, DLC, exact frame
  bytes, sequence, command, and the independently calculated CRC-8 result; no
  CAN ISR performs formatted output.
- PC Detailed trace now labels the matching frames `CUSTOM_TRANSFER` and adds
  `crc=OK/BAD`. For evidence, compare PC TX `0x710` with target RX `0x710`, and
  target TX `0x711` with PC RX `0x711`; endpoint timestamps are not keys.
- F13 Transfer CS+ `/br` PASS: Error 0 / Warning 2, PROGRAM `0x49D7`, ROM data
  `0x04BF`, RAM data `0x00CC`. Disable `CAN_TRANSFER_UART_TRACE_ENABLE` before
  timing or queue-stress measurements because 115200-baud text is blocking.
- PC x64 Release and Debug builds PASS with 0 warnings / 0 errors; both unit-test
  runs PASS. The delivery executable is Release build `0.1.0.0052`, and startup
  smoke reports the same window title. The 16-diagram,
  21-page Traditional Chinese PDF was regenerated directly under `docs` and
  all rendered pages, including the new page 19 comparison sequence, passed
  visual inspection.
- Physical dual-log comparison and an external CAN trace remain release gates.

Custom Transfer timeout-diagnostic slice (2026-10-14):

- The supplied hardware log proves Host Link/F24 probe works and each failed
  `CAN_TX_WAIT_RX` attempt advances F24 TX by two (original plus retry), while
  F24 RX remains zero. Repeated channel source `0x0001` is `BUS_ERROR`; `0x0003`
  is `BUS_ERROR|ERROR_WARNING`. This occurs below the target application
  protocol and does not by itself implicate the exact acceptance filter.
- F13 Transfer now prints `CAN init: OK/FAIL`, driver result, channel status,
  compiled 500K IDs/filter, and P12 standby at startup. Its ISR only coalesces
  error flags; the main loop emits decoded `TRACE CT CAN_ERROR` output at most
  once per 250 ms during a repeated fault.
- PC asynchronous diagnostic logs now preserve the raw F24 values and append
  readable channel/global source names. No Host Link payload changed.
- F13 Transfer CS+ rebuild PASS: Error 0 / Warning 2, PROGRAM `0x4C4A`, ROM data
  `0x0663`, RAM data `0x00D2`. PC Release build `0.1.0.0054` PASS with 0 warnings
  / 0 errors; Release tests and startup smoke PASS. The regenerated Traditional
  Chinese PDF remains 21 nonblank pages/16 diagrams; rendered contact sheets
  and the full-size changed pages pass visual inspection.
- Reflash the F13 target and repeat discovery while saving UART. The remaining
  decision tree is: init FAIL -> controller setup; init OK/no RX -> wiring,
  ground, transceiver standby/power, termination, or actual bitrate; RX/no TX ->
  target processing; RX+TX/no host RX -> return path or F24 RX/filter.

## Confirmed initial settings

- F24 Classical CAN: 500 kbit/s.
- F24 CAN FD: 500 kbit/s nominal, 2 Mbit/s data.
- F13 projects use Classical CAN 500 kbit/s in their active configurations.
- F13 Boot request/response IDs: `0x700` / `0x701`.
- Current F13 metadata declares app start `0x2000`, capacity `0x1E000`, format
  3, protocol 3 and a variable 4-byte-aligned actual image; CRC32 is in the
  final four bytes. These values are not PC-tool global limits.
- Host F13 Boot RX filter: `0x701` / `0x7FF`, standard data frame.
- F13 Transfer request/response IDs: `0x710` / `0x711`; exact standard-data
  response filter `0x711` / `0x7FF`.
- No accept-all filter mode.
- Host Link maximum payload: 96 bytes.
- First release: one CAN channel and one target. F24 supports one active plus
  two pending generic CAN requests; current F13 workflows deliberately issue
  one target transaction at a time.

## Open decisions

- Maximum acceptable full F13 update time.
- Final button actions beyond input diagnostics.
- Final error-LED clearing policy after hardware testing.

The first-release security policy is decided: CRC32 integrity and target/image
identity checks are required; digital signatures, authentication, and
anti-rollback are out of scope.

## Next implementation steps

1. Resolve the remaining historical CS+ device-change/I/O-header notice if the
   IDE continues to request generation; do not hand-edit generated sources.
2. Verify physical PC-to-F24 `PING`/`GET_HOST_INFO`, then configure
   the F13 Transfer `0x711 / 0x7FF` filter and capture an end-to-end trace.
3. Run the worker-side F13 Transfer discovery/test workflow and capture
   payload-integrity, wrong-ID, timeout, duplicate, and error traces.
4. Run the worker-side F13 Boot HELLO/explicit-ENTER flow on hardware, then
   capture a complete update trace including erase-boundary latency and END
   CRC/reset behavior.
5. Inject DATA timeout, disconnect/cancel, wrong image/target, flash error, CRC
   error, bus-off, and power interruption; verify ABORT/inactivity recovery.
6. Verify power-loss/shared-vector/FSL recovery and confirm the programmed WDT
   option bytes remain disabled as required by the product policy.

Update this file after each completed development slice with changed files,
build/test commands, actual results, and remaining risks.

### 2026-10-14 - Debug/Release output isolation

- Root cause of the alternating approximately 13-MB/2-MB delivery EXE was a
  shared project `OutDir`: Debug overwrote `build\FirmwareUpgradeTool.exe`, while a
  later incremental Release build could skip relinking.
- Release x64 now compiles to `tmp\internal\build\release\x64\FirmwareUpgradeTool.exe`; Debug
  writes `tmp\internal\build\debug\<platform>\FirmwareUpgradeTool.exe`, and Release Win32 writes
  `tmp\internal\build\release\Win32\FirmwareUpgradeTool.exe`.
- After a successful Release x64 build, the build script copies the compiled
  EXE to the stable delivery path `build\FirmwareUpgradeTool.exe`. No MSBuild
  configuration owns that delivery path, so Debug clean/rebuild cannot remove
  it. The script increments the product version only for Release builds.
- Verification PASS: Release `0.1.0.0058` is 2,928,640 bytes; Debug is
  13,401,088 bytes. After a full Debug x64 Rebuild, the Release delivery EXE
  remained present with unchanged SHA-256. Release/Debug protocol tests,
  Release startup smoke, and the 24-script clean audit also PASS.

### 2026-10-14 - Standalone periodic F24 TX diagnostic

- PCAN still captured `0x000`, DLC `0` at approximately 500-ms retry intervals
  while Tool detailed trace encoded `0x710`, DLC 8 correctly. F24 reported TX
  completion and no warning/passive/bus-off entry in the latest run.
- Tool `0.1.0.0056` provides `Start/Stop F24 TX Test`. It configures Classical CAN
  500K and sends standard ID `0x123`, DLC 8, data
  `11 22 33 44 55 66 77 XX` every 1000 ms through generic `CAN_TX`, without an
  F13 target or response wait. `XX` resets to `00` for each Start and increments
  per frame. The adjacent `?` tooltip explains the analyzer requirement.
- Stop, Disconnect, and shutdown are bounded and call CAN stop cleanup. Each
  completion logs the transmit count, F24 timestamp, and controller result.
- Physical analyzer verification remains open. Correct output must appear as
  `0x123`, DLC 8 with the exact data; repeated `0x000`, DLC 0 localizes the
  remaining fault to the F24 TX path rather than the F13 protocol.
- Release and Debug x64 builds and both protocol-test binaries PASS. Release
  startup smoke opened a responsive `0.1.0.0056` main window. Native-window
  screenshot capture was unavailable in this session, so final visual layout
  inspection remains NOT RUN.

- The user-facing name is now `Start/Stop Bridge Board Self Test`, because the
  diagnostic belongs to the generic Host Link bridge contract and must remain
  usable by future F14 and RA6 bridge-board implementations. Historical F24
  evidence and the current F24-specific firmware diagnosis retain their MCU
  name where technically relevant.

### 2026-10-14 - F24 CAN TX descriptor correction

- Logs `can_upgrade_tool_20261014_143957.log` and
  `can_upgrade_tool_20261014_145304.log` prove Host Link v2, CAN setup, and TX
  scheduling execute, while PCAN-View captured the initial request and retry as
  Classical ID `0x000`, DLC `0` instead of ID `0x710`, DLC `8`.
- `host_link_protocol.c` now constructs the Renesas packed TX header words and
  verifies their bit-field view before accepting a request. Active/queued
  transaction and frame copies now use explicit `memcpy`.
- F24 clean rebuild PASS: firmware `1.0.0 build 20`, Error 0 / Warning 2,
  PROGRAM `0x4D12`, ROMDATA `0x0275`, RAMDATA `0x0919`; both warnings are the
  existing CC-RL paid-license warnings.
- Hardware retest remains open: reflash build 20 and confirm PCAN sees standard
  ID `0x710`, DLC `8`, payload beginning `A5 01`, then F13 `TRACE CT RX` and
  response ID `0x711`.

### 2026-10-14 - Generic bridge self test and F24 frame construction follow-up

- The Tool button, status, errors, and logs now use `Bridge Board Self Test`
  instead of the F24-specific name. The Host Link operation remains generic so
  future F14 and RA6 bridge boards can implement the same contract.
- Hardware evidence from F24 firmware build 22 shows valid error-free frames at
  the expected one-second period, but PCAN receives ID `0x000`, DLC 0 instead
  of the Tool-requested ID `0x123`, DLC 8 and payload. This proves the bitrate,
  transceiver, and scheduling path work while TX message content is wrong.
- Both provided Renesas F24 CAN examples populate `st_can_tx_frame_t.Head.Bits`
  directly before calling `R_CAN_SendByTXMB`. The bridge decoder now follows
  that known example rather than manually constructing packed `Head.Word[]`.
  F24 build 23 clean rebuild PASS, Error 0 / Warning 2, PROGRAM `0x4D63`,
  ROM data `0x0275`, RAM data `0x0919`. Hardware retest after reflashing build
  23 remains open.
- PC Tool `0.1.0.0059` Release/Debug x64 builds PASS with 0 warnings/errors;
  both protocol-test binaries, Release startup smoke, and the 24-script clean
  audit PASS. Final visual confirmation of the longer button caption remains
  NOT RUN in this session.

### 2026-10-14 - F24 TX message-RAM read-back diagnostic

- The supplied build-26 hardware log confirms 27 successful TX completions and
  27 analyzer frames, but every physical frame remains ID `0x000`, DLC 0.
  Renesas example-style bit-field construction therefore did not alter the
  hardware result and header construction alone is ruled out.
- F24 build 27 captures the header/data presented to the driver and reads back
  TX buffer 0 ID, DLC, FD control, first eight data bytes, and RAM-window value
  immediately after `R_CAN_SendByTXMB`. Binary Host Link UART remains free of
  `printf` output.
- `CAN_TX` retains its original first eight response bytes and may append a
  32-byte diagnostic extension. Tool accepts both 8-byte legacy and 40-byte
  extended responses and emits `Bridge TX diagnostic` during the self test.
- F24 clean rebuild PASS, Error 0 / Warning 2, PROGRAM `0x4E4D`, ROM data
  `0x0275`, RAM data `0x0939`. Tool `0.1.0.0061` Release/Debug PASS with 0
  warnings/errors; extended-response tests, Release startup smoke, and the
  24-script clean audit PASS. Reflash/build-27 read-back remains NOT RUN.

### 2026-10-14 - CAN RAM failure traced to invalid F24 clocks

- Hardware log `can_upgrade_tool_20261014_182125.log` identifies firmware build
  30 and Tool 0061. All 13 driver-input frames have the correct 0x123 ID, DLC 8
  and counter payload. TX-RAM read-back is invalid on the first frame and zero
  on subsequent frames; TX completion still reports success. The saved window
  value 0 is normal restoration, not a page-selection failure indicator.
- Renesas Hardware Manual Rev.1.20, chapter 18.17 requires fCLK=fMP/2 and
  prohibits HOCO as the CAN communication clock. The old HOCO 40 MHz/MDIV=0
  violated both requirements. Earlier frame-format fixes did not address this.
- Visually inspected the board schematic's RL78 sheet: Y3 is 4 MHz, connected
  to P121/P122. Added hand-maintained `f24_can_clock.c/.h`, outside generated
  code. Startup stays HOCO 40 MHz to reach the debugger's main breakpoint;
  hardware_init then switches X1 4 MHz -> PLL 80 MHz -> MDIV=1 before starting
  UART/timer. CPU/UART/timer remain 40 MHz. X1/PLL waits are bounded, with HOCO
  fallback and CAN rejection on failure.
- CAN start/transmit validate the runtime clock. SET_CAN_CONFIG may report an
  eight-byte C1 clock-failure diagnostic; Tool 0062 names the failing stage.
  Legacy success responses and TX diagnostics are preserved.
- Verification commands: CS+ `/br`; Release/Debug `scripts/build_mfc.ps1`;
  both HostLinkProtocolTests executables. The clock test compiles the real
  embedded clock module against mock registers and covers X1, main selection,
  PLL lock/selection timeouts and restoration, normal/repeated initialization,
  clock loss, and unexpected startup. Protocol tests cover clock-error wording
  and original 8-byte/extended 40-byte CAN_TX responses. All tests PASS.
- Physical confirmation of clock switching, fallback, debugger main stop and
  analyzer ID/DLC/data remains pending. Do not claim the bus issue is resolved
  until a new firmware/PCAN trace demonstrates correct RAM and wire content.
- Final verification: F24 build 32 `/br` PASS, Error 0 / Warning 2 (license),
  PROGRAM `0x512F`, ROMDATA `0x0276`, RAMDATA `0x093A`; Tool 0.1.0.0062
  Release/Debug builds have 0 warnings/errors and both test binaries PASS.
  Release/Debug startup smokes opened responsive 0062 windows and closed
  normally; workspace audit PASS (24 clean-related scripts). CS+ retains the
  historical device-change notice. Full changed-file list is recorded in the
  corresponding DEVELOPMENT_CHECKLIST entry.

### 2026-10-14 - User-regenerated 20-MHz X1 and write-once startup fix

- Completed: reviewed the user's regenerated SC configuration, UART/timer/
  button callbacks, and build-34 hardware log `185719.log`. The HXT setting
  is now X1 20 MHz, but generated startup waits for X1 before main, PIOR46
  reverted to P73/P72, and the inherited CAN FD SJW 17/7 exceeds TSEG2 16/6.
- Replaced the old runtime CMC rewrite: CMC is write-once after reset. CS+
  preincludes the new hand-maintained `f24_board_config.h` in every C unit.
  It loads generated headers, defers X1 startup, fixes PIOR46=0 and SJW=16/6.
  No generated driver or configuration file was edited. SHA-256 comparison
  confirms all 66 files under src/smc_gen are unchanged by this work.
- Runtime clock: X1 20 MHz /4 x16 -> PLL 80 MHz -> MDIV=1 -> CPU/CAN 40 MHz.
  PLLCTL=E2 before start/E7 after selection; divider pair is set atomically.
  CMC=51 is set by BSP, never runtime. Fallback stops PLL before changing
  MCM0. UART remains 115200; nominal/data remain 500K/2M.
- Changed files: firmware `f24_board_config.h`, `f24_can_clock.c/.h`,
  `RL78_F24_CAN_host_upgrade.mtpj`; `tests/f24_can_clock_tests.c`,
  `tests/f24_clock_mock/platform.h`; AGENTS/README/checklist/handoff/self-check,
  firmware README and F24 bring-up/Host Link protocol documents. CS+ also
  increments the existing build counter/generated version header normally.
- Verification: final CS+ `/br` is firmware 1.0.0 build 37, Error 0 /
  Warning 2 (existing license), PROGRAM 0x517F, ROMDATA 0x0276, RAMDATA 0x093A.
  Log: tmp/internal/build/logs/f24_hxt20_final_build_20261014.log. CC-RL -P/-S outputs under
  tmp/internal/build/verify confirm CMC=51 once, MSTOP=1 and no X1 OSTC wait before main,
  PIOR46=0 and P11/P10 driver setup, and effective NSJW=15/DSJW=5.
- Release/Debug tests PASS with 0 warnings/errors: real clock module and
  generated configuration, read-only CMC mock, invalid locked CMC, PLL
  divider/multiplier loss, normal/repeated setup, and timeout/fallback.
  One-click Debug x64 build PASS; clean-script audit PASS (24 scripts).
  PC production source/Release EXE unchanged; Tool 0062 remains compatible.
  GUI smoke NOT RUN this slice (native Computer Use pipe unavailable).
- Physical results: build-34 log proves 115200 host probe and RESET_HOST ACK/
  bounded reconnect pass, but CAN never started (CMC=0, stage 2). Build 37
  debugger/20-MHz clock/CAN output is NOT RUN. Next: flash the current MOT on
  the confirmed 20-MHz board, probe, then self-test with active PCAN 500K;
  expect ID 123/DLC8/data 11 22 33 44 55 66 77 XX and matching TX-RAM fields.
- Future 250K/1M selection is a design note, not implemented: SET_CAN_CONFIG
  travels over USB-UART, so no CAN-500K command bootstrap is needed. Add
  bridge timing/capability/UI/tests together; the device must use the same rate.

### 2026-10-14 - UART-configured 250K/500K/1M presets

- Implemented wire presets 1/2/4 (3 is reserved for 800K). F24 capabilities
  nominal=0x16, data=0x02. New hand-maintained f24_can_timing.c/.h packs actual
  nominal registers at fCAN40M: 250K H/L=1E7C/3C01, 500K=1E7C/3C00,
  1M=0E3C/1C00; 80% sample points and SJW<=TSEG2. Data stays generated 2M.
- SET_CAN_CONFIG checks stopped state, clock and global/channel reset before
  writing timing. CAN_START reapplies after R_CAN_SetConfig to avoid a silent
  return to generated 500K. UART baud, X1 20M/PLL80/CPU40, P11/P10 and WDT
  policy are unchanged. No generated driver was modified.
- Tool dropdown is capability-filtered, with 500K default and existing INI
  selection restore. Mode/nominal changes enqueue UART SET_CAN_CONFIG on the
  worker, leave CAN stopped, and require no CAN500K bootstrap handshake.
  Self Test and generic target setup use the selected rate. Current F13
  metadata remains 500K-only; bridge changes never reconfigure the target.
- Changed files: F24 f24_can_timing.c/.h, can_fd_module.c, host_link_protocol.c,
  CS+ mtpj; PC core/can_host_client.c(pp)/h, worker_session.cpp/h,
  main_frame.cpp/h and main_frame_workflow.cpp; tests host_link_protocol_tests.cpp,
  f24_can_clock_tests.c, f24_clock_mock/platform.h and test vcxproj;
  AGENTS/README/SELF_CHECK/DEVELOPMENT_CHECKLIST/HANDOFF, firmware README,
  Host Link protocol, architecture, F24 bring-up and diagnostics docs. Existing
  firmware/tool version files are updated by their normal build scripts.
- Verification: F24 build 41 clean /br PASS, 0 errors/2 existing license
  warnings; PROGRAM52E9, ROMDATA0276, RAMDATA093A. Log:
  tmp/internal/build/logs/f24_uart_bitrate_presets_20261014.log. Tool 0.1.0.0063 Release/Debug
  one-click builds PASS, 0 warnings/errors; both test binaries PASS. Tests
  execute the real C timing module, all 253 invalid preset values, decoded
  timing/rate/SJW, protected register and missing-clock cases; PC tests cover
  all six Classical/FD payloads, mapping, old host masks and zero-traffic
  rejection. Workspace audit PASS (24 scripts). Map has no printf/putchar.
- GUI startup/visual smoke NOT RUN: Computer Use native pipe unavailable
  after retry/reset. Native layout dimensions are unchanged. Hardware UART
  commands, bit timing, FD data, analyzer frames, rate changes after stop and
  end-to-end F13 transfers remain NOT RUN. Do not mark release or hardware
  gates complete from these software results.
- Next hardware check: flash build41, use Tool0063 Open+Probe, stop any active
  workflow, choose Classical 250K/500K/1M, confirm UART cmd10 echo ACK, set
  analyzer to the same rate and Start Bridge Board Self Test. Expect ID123 /
  DLC8 / 11 22 33 44 55 66 77 XX. Disconnect other nodes using a different
  bitrate for this bench test. Current F13 transfer/update must remain at 500K.

### 2026-10-14 - COM friendly names and confirmed 500K Custom Transfer

- Implemented COM selector labels such as `COM6: USB Serial Port (COM6)` using
  read-only SetupAPI FriendlyName/DeviceDesc and registry PortName metadata.
  QueryDosDevice retains unnamed ports; numeric sorting/deduplication and
  canonical item data prevent labels from reaching CreateFile or the INI.
  Refresh retains the selected COM number, and reset/reconnect is unchanged.
  Adapter names do not prove MCU identity; Open + Probe Host remains required.
- COM field uses measured item widths, 340-520 px, with a wider dropdown.
  Open/Close/Reset wrap below the selector when the row cannot fit them plus
  build information. Layout now waits until the last control is created.
  Queued probe logs include the selected Windows adapter description.
- Changed files: `src/core/serial_port_catalog.cpp/.h`, `src/main_frame.cpp/.h`,
  `src/main_frame_layout.cpp`, `src/main_frame_workflow.cpp`,
  `FirmwareUpgradeTool.vcxproj/.filters`, `tests/serial_port_catalog_tests.cpp`,
  `tests/host_link_protocol_tests.cpp`, `tests/HostLinkProtocolTests.vcxproj`,
  AGENTS/README/SELF_CHECK/DEVELOPMENT_CHECKLIST/HANDOFF and architecture/
  diagnostics docs. Normal build scripts update existing tool version files.
  No MCU firmware or legacy simple_serial_to_CAN_tool files were changed.
- Verification: one-click Release and Debug x64 builds PASS, 0 warnings/errors,
  final Tool `0.1.0.0065`, build `2026-10-14 20:22:27`. Delivery EXE is
  `build/FirmwareUpgradeTool.exe`, 2,950,144 bytes. The initial 0064 build hit an
  int/LONG text-width comparison error; an explicit conversion fixes it.
  Release/Debug full unit/mock tests PASS, including friendly-name/Unicode,
  absent/invalid/duplicate/high-number ports, fallback and numeric ordering.
  Read-only real enumeration returns 0 COM ports in this environment; attached
  adapter-name lookup and live INI/refresh behavior are NOT RUN here.
  Workspace clean-script audit PASS (24 scripts).
- Computer Use skill was used for UI verification initialization and prescribed
  retry/reset recovery. All list_apps calls fail with native-pipe missing
  (os error 2); Release/Debug launch, screenshots and 100%/125% visual validation
  are NOT RUN. No alternative direct PowerShell UI automation was used.
- Hardware evidence: `build/can_upgrade_tool_20261014_201510.log`, Tool0063 /
  F24 `1.0.0 build40`, UART115200, Classical500K, exact RX711/7FF, passes all
  seven DEVICE_INFO pages, PING, ECHO, PATTERN_TEST and twelve GET_STATS reads.
  Twenty-two TX/RX pairs match the supplied F13 UART sequence1..22. F24 final
  RX22/TX22, lost/failures/warning/passive/bus-off/recovery all zero. Target
  reported lost/protocol/CRC/duplicate/state-error counters are zero. Physical
  CLEAR_STATS, boundary/stress/negative tests and Boot/App updates remain open.
- F13 board root cause is recorded: QB-R5F10BMG-TB IC2 TJA1050T VCC uses EVDD,
  requires 4.75-5.25V, and S/P12 LOW enables normal TX. User measured 3.3V, then
  supplied PCAN/F13 logs showing correct requests/responses and no errors at
  5V. Existing startup text calls S `STB`; no embedded source was altered here.
- Next: open 0065 on the user's connected PC, confirm COM adapter names and
  preserved selection after Refresh/restart, inspect normal/restored layouts
  at 100%/125% DPI, then continue separate Boot/App and fault-test gates.

### 2026-10-14 - F13 boot/app aligned to the proven Transfer benchmark

- Resume/source-of-truth audit: inspected the user-supplied Transfer pcan.jpg
  and log.jpg plus Tool201510.log. Transfer is the working communication
  benchmark (22 matched requests/replies, final bridge CAN error counts zero),
  not proof of boot/app programming. Compared active clock, routing, driver,
  timing and generated startup paths before changing either target project.
- Added shared strict-C90 `f13_board_support.c/.h` in the boot project,
  compiled by both CS+ projects. X1=4MHz /2 x16 -> CPU32 (MDIV0), CAN16;
  existing CAN500K/75% timing and UART115200 remain. CMC is prepared once
  before RAM initialization; bounded X1/PLL waits run in main, not pre-main.
  A valid inherited PLL is preserved on boot-to-app jump. Failed clock
  initialization prevents CAN/FSL use. CAN prepare silences S/P12, masks IRQs,
  waits for RAM, resets global then channel modes before timing/filter setup;
  this handles an already-operating controller inherited from boot.
- Boot polling UART logs now retain TX/RX IRQ masks: those UART vectors route
  to app handlers and must not execute while app RAM is uninitialized. App
  request processing is gated by successful CAN initialization and clock-ready.
  FSL still uses the F13 library and 32MHz, never F24 clock/library settings.
  No WDT was introduced; flash addresses, ISR/no-init contracts and CAN IDs
  remain unchanged (boot/app700/701, Transfer710/711).
- Identity is now paired boot/app1.1.1 (0x00010101), build20261014. Initial
  board setup must use the new paired app-project `boot_app.hex`; do not mix
  this boot with the previous HOCO-only valid app, which may alter inherited
  PLL configuration. Boot-only recovery is valid with an erased/invalid app.
  QB-R5F10BMG-TB TJA1050T requires EVDD4.75-5.25V and S/P12 LOW; the startup
  EVDD requirement text is not an ADC voltage measurement.
- Changed embedded files: boot `f13_board_support.c/.h` (new), `r_main.c`,
  `can_boot_transport.c`, `fsl_flash_interface.c`, `user_def.h`,
  `can_boot_config.h`, `RL78_F13_Boot_loader_CAN.mtpj`, `README.md`; app
  `custom_func.c`, `src/code_gen/r_systeminit.c` (preserved include area only),
  `ftable.inc`, `RL78_F13_Boot_loader_CAN_app.mtpj`, `README.md`. Normal CS+
  rebuild/post-build updates maps/HEX/BIN/combined image. Generated app CGC
  source remains untouched and inactive through the preserved startup hook.
- Changed automation: `tests/f13_board_mock.h`,
  `tests/f13_board_support_tests.c` (new), `tests/host_link_protocol_tests.cpp`,
  `tests/HostLinkProtocolTests.vcxproj`, `scripts/verify_f13_artifacts.ps1`.
  Real C source mocks cover all clock stages/fallback failure, wrong CMC and
  divider, CAN RAM/global/channel timeout, silent S on failure, pre-main
  no-wait/no-global behavior and inherited PLL/operating CAN. Artifact checker
  additionally requires exactly one shared-source registration per CS+ project
  and active boot/app shared-clock/CAN calls plus the app preserved-user hook.
- Changed docs: Tool AGENTS/README/SELF_CHECK/DEVELOPMENT_CHECKLIST/HANDOFF,
  `docs/ARCHITECTURE_V2.md`, `docs/F13_BOOT_UPDATE_FLOW_zh-TW.md` and guide PDF.
  No Transfer reference firmware, F24 firmware or PC production UI source was
  changed. Transfer/boot/app CAN driver and SFR-header SHA256 values remain
  identical; Transfer clock source was not altered.
- Actual verification: final CS+ `/br` boot clean build 21:47:50-21:47:55,
  Error0/Warning5, PROGRAM1A0B/ROMDATA0430/RAMDATA092E; app clean build
  21:49:01-21:49:12, Error0/Warning3, PROGRAM4713/ROMDATA05B7/RAMDATA00B5.
  Remaining warnings are existing evaluation-license, vector/divided-HEX and
  CC-RL migration messages, not new source diagnostics. Final artifact checker
  (`scripts/verify_f13_artifacts.ps1`) PASS: boot/FSL maximum end1C85,
  margin37A=890bytes below2000, map-derived app end/CRC address6AD4,
  update BIN4AD8=19160bytes, lastbyte6AD7, CRC32=5AAE684B, version10101,
  manifest/layout/identity/shared-vector checks and all new source guards PASS.
- Actual PC verification: Release test-project build and
  `scripts/build_mfc.ps1 -Configuration Debug -Platform x64
  -CloseRunningTool $false` PASS with 0 warnings/errors. Executed both
  `tmp/internal/build/tests/x64/{Release,Debug}/HostLinkProtocolTests.exe`: full tests PASS,
  including new real-source C mocks. No production Release rebuild/version
  bump was needed; delivery Tool remains0.1.0.0065. Workspace
  `#READ_ME_FIRST#/scripts/verify_clean_scripts.ps1` PASS (24 scripts).
- PDF skill used to regenerate the existing docs-only deliverable via
  `scripts/generate_f13_guide_pdf.ps1`: 16 Mermaid diagrams render, 22 nonblank
  PDF pages, embedded fonts. All pages rendered with PyMuPDF (Poppler not
  available locally), contact sheets and affected full-size pages inspected.
  Simplified the boot decision diagram without changing its branches so it
  remains legible. No separate output/pdf directory was created.
- How to verify next: power off before supply changes, ensure 5-V EVDD and
  compatible UART levels, flash the paired boot_app.hex and leave debug mode.
  Tool0065 Open+Probe -> CAN Upgrade/Classical500K -> Discover Target;
  boot/app request700 response701, not Custom Transfer. Read-only analyzer
  INFO is STD700/DLC2/07 00, expected STD701/DLC8 page0
  `07 00 00 02 3B F1 03 03` in boot or `07 00 00 01 3B F1 03 03` in app.
  Load the newly generated update BIN, confirm size19160/CRC5AAE684B/version
  1.1.1, Enter Bootloader, then Start Firmware Update and capture tool/UART/
  analyzer logs. New clock/CAN-ready logs must report successful init.
- NOT RUN: new boot/app physical clock/CAN response, INFO/ENTER/update,
  E2 Lite halt/Continue, shared ISR runtime, power-cycle/power-loss and FSL
  fault injection. These hardware/release gates remain unchecked. Existing
  native-tool UI/DPI verification limitations are unchanged by this task.


