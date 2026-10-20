# Firmware Upgrade Tool Development Checklist

Public documentation follows docs/PUBLIC_CONTENT_GUIDELINES_zh-TW.md.
This is a generated copy of the generic /web root policy; keep MCU/project
rules local. F13/F24 CAN device Boot/App are joint golden samples; metadata
and IRQ input are C headers, with identical standalone readers and no Python
firmware-build dependency. Verify both App builds/negative cases/A-B flows
when changing common authoring behavior; see docs/DEVICE_GOLDEN_SAMPLE_zh-TW.md.
For MD/web/PDF/README changes, sync the standalone slide rules and affected
canonical sections; run mirror/content checks and review contextual meaning.
Preserve technical/safety constraints and record actual render/link results.

This document is the ordered implementation and verification checklist for the
PC tool, the RL78/F24 CAN host firmware, and the RL78/F13 target projects.

## Status legend

2026-10-20 three-entry review:ISP/Bootloader/knowledge-base page intros classify
Classical F13/F24,CAN FD F24,and Custom Transfer across all three. Canonical
Transfer chapter table now contains verified source/operation links,with strict
two-block sync.46production guards,syntax,idempotence,310refs/home anchors,
18-file content/MD-HTML parity/four diff checks PASS. MultiBus PDF16pages/four
diagrams and six URI checks/all-page visual review PASS. Existing FD engineering
body retained. Browser preview NOT RUN(no CUA browser surfaces);own helper stopped.
No runtime/firmware change,product build/version or Git/website publication.

2026-10-20 CAN FD overview:canonical resource/workflow tables link existing FD
engineering/update/Custom Transfer chapters;Classical labels and publisher scope
clear. Single-owner MD/web CheckOnly,310refs/anchors,18-file public checks and
production guard positive/three negative mutations PASS. MultiBus PDF16pages/
four diagrams rendered/all pages reviewed;FD PDF links checked. All11current
deliveries present;Tool EXE/10surviving firmware hashes unchanged. No rebuilt
intermediates,product change or publication;browser/DPI preview NOT RUN.

2026-10-20 FD publication: nine repositories/eleven delivery files/ten archive
groups;path mappings,ignore example and preflight guidance synchronized.
19 isolated Git cases/four path tests PASS. FD generator bookkeeping/comments
portable without SFR or compiler-setting changes;three preparation CheckOnly
and relocated sibling checks PASS. README retains App/Transfer dependency on
same-parent FD Boot. User133221 normal App->ENTER->update/App/UART confirmed,
seven Host error deltas zero;stress/power remain separate. MD/web/310refs/
18-file content/33safe-clean and13-page/four-flow PDF visual review PASS.
Tool EXE/16firmware artifact hashes unchanged;no rebuild/version bump or real
Git publication. Actual preview stops before cleanup because FD .git is absent.
Manual CS+ GUI/SC regeneration and compiler rebuild NOT RUN in this task.

2026-10-20 Tool0097:1000ms cancel-aware no-CAN-request wait after END,
original12-second probe budget;warning/passive included in operation deltas.
Confirmed updates with Host error increases or STOP warnings produce amber
PASS_WITH_WARNINGS;FAIL/CANCELED/unavailable snapshots remain distinct.
Debug/Release builds/startup/native suites,FD models,7integration/5PDF tests,
MD/web/310refs/content/33safe-clean and16+13page PDF visual review PASS.
16firmware artifacts unchanged. User130843 confirms one normal FD update/
App/IRQ/counter with warning/passive+1,not0097 restart-error elimination.
Existing observability artifact check remains1FAIL(pruned F24 intermediate)
and1SKIP(absent F13 map);no golden rebuild. Hardware repeat/fault NOT RUN.

2026-10-20 FD device pair:compact RXMB0 ISR/8-entry queue,AA55 full Boot4C /
App2098 route,driver vect ISR0 with explicit hardware IRQ. Boot fixed8 reply
and64-word vector validation preserve guards. Clean Boot0errors/2warnings,
end1DBF/spare576;App0errors/5warnings,4756bytes/CRC9819B975,minimum Boot1.2.0.
9artifact/actual-config/C90 per-profile512capture/queue/fault/64slot models,
PC/host/device RAM retry/replay/lostACK/padding/disconnect and Transfer models
PASS. Transfer PP/516protected hashes unchanged. MD/Web/310refs/content/
33safe-clean/syntax,13-page PDF/4diagrams/all-page visual review PASS.
User112409 verifies standalone IRQ normal traffic only;FD Boot/App physical
update/repeat/fault/power NOT RUN. No Tool/Host/Transfer rebuild or publication.

2026-10-20 FD Transfer RXMB IRQ:unchanged vendor ISR/vector4C + InHandler,
8-entry SPSC queue,main-loop protocol/logging,no RX polling fallback. TX unchanged.
512actual vendor ISR/API native receives,queue wrap/full/order/masked IRQ,
overwrite/errors/bounds/saturation/stop/reinit/window restore and PC trace0/1
PASS. Clean standalone0errors/2license warnings;HEX/MOT range/ISR/banner/API
and vendor SHA verified.9artifact/compact HAL/metadata/CRC/block models PASS;
Boot/App PP unchanged,517protected hashes unchanged. MD/Web/CheckOnly/310refs/
content/26safe-clean and11-page PDF/3diagrams/all-page visual review PASS.
User polling baseline passed Discover+4tests/79RX/TX and is archived;IRQ
hardware NOT RUN. No Tool/Host/Boot/App rebuild,COM/Flash or Git publication.

2026-10-20 FD Transfer RXMB0 path:one64-byte RXMB,exact0x710,main-loop vendor
R_CAN_ReadRXMB;receive FIFOs/interrupts disabled only for Transfer. Clock/
rates/IDs/TX/wire unchanged. Actual vendor C90 model128mixed receptions with
busy/overwrite/bounds/flags/no FIFO write and PC/protocol trace0/1 PASS.
Effective config/timing guard and Boot/App preprocessing isolation PASS;
517protected hashes unchanged. Clean standalone0errors/2license warnings,
9artifact/compact HAL/metadata/block models PASS. HEX/banner/linked RXMB
verified;MD/web/310refs/content/26safe-clean and9-page PDF visual QA PASS.
Physical RXMB acceptance/root cause NOT RUN. Earlier FIFO entry below is an
engineering record,not the current standalone receive backend.

2026-10-20 FD Transfer vendor FIFO path: Transfer-only adapter calls the real
R_CAN_ReadRXFIFO, preserving16x64 FIFO/mode/rates/IDs/TX/protocol. Actual vendor
body/tables/RX types are extracted for C90 register tests;128 modeled dequeues,
8/64-byte frames,empty/lost/error/DLC/flags/no-extra-pop checks and trace0/1 PC
protocol models PASS. Three clean Transfer builds0errors/2license warnings PASS.
Non-Transfer adapter preprocessing unchanged. Tool/Host/Boot/App are not build
targets.517protected hashes unchanged;actual HEX banner/vendor API linked.
MD/web/CheckOnly,9-page PDF/all-page visual check,310refs/content/26safe-clean
and PowerShell syntax PASS. Physical repeated-session acceptance/root cause
remains NOT RUN.

2026-10-19 FD runtime diagnosis: Tool0096 decodes returned21 timeout attempts/
result,uses bus-layer guidance and adds failed stats index. FD Transfer adds
read-only/bounded STATE/REGS checkpoints,idle/TX-failure snapshots. C90 models,
128pop-write observations,trace0/1,Debug/Release native regressions,FD9guards
and clean builds PASS. Startup/normal close PASS;Classical512hashes,Host/FD
Boot/App products and non-Transfer backend preprocessing unchanged. MD/web/
9-page PDF/content/310refs/24safe-clean PASS. Physical cause/new diagnostic
capture unresolved;user traces verify only discovery/base/FD64 partial test.

2026-10-19 FD Custom Transfer: Tool0095 removes Classical-only discovery and
uses strict FD+BRS sessions; FD Transfer has isolated FD backend/hooks and
64-byte ECHO/CRC/replay. Transfer preflight uses no BIN/Boot/burst requirement.
Two device clean builds,actual C90/PC models with trace0/1,native Debug/Release
Classical and FD upgrade regressions PASS. Release build/start/normal close
PASS;512 Classical files and Host/FD Boot/App delivery hashes unchanged.
FD operation/protocol/architecture/MD/web and8-page/2-Mermaid PDF aligned;
visual/syntax/310references/24safe-clean checks PASS. Physical FD NOT RUN.

2026-10-19 CAN FD: isolated F24 device Boot/App500K/2M/BRS/FD64,
generic Host App bit13/command22 version2 and mode-specific Tool gates built.
Classical golden512-file hashes and Host Boot unchanged. Nine artifact tests,
C90 HAL/metadata/block and production PC/host codec/device RAM FD models PASS;
native Debug/Release Classical regressions PASS. Boot spare522>=512.
Tool0094 Release/start/normal close PASS;Host App clean build PASS.
FD MD/web/protocol/5-page PDF synchronized and visually checked;reference/
content/24safe-clean checks PASS. Physical FD update/fault/power NOT RUN.

2026-10-19 F24 device README alignment: all three public READMEs are English
with corresponding F13 reader structure; local F24 hardware/IRQ/metadata/
build/output/capability facts preserved. Five read-only source/link tests PASS.
Full tutorials remain linked, not duplicated; no F13 or firmware code edit,
rebuild, hardware activity or commit/push. Three-README public checks and
final six-repo read-only preview PASS; CRC8 example frames verified.

2026-10-19 tracked local-data recovery: user-approved --cached removes15 F24
Boot tmp paths while preserving local files; three device ignores added.
Four script-owned staged deletions cleared with index backups; all eight
deliveries/actual HEADs unchanged. Preflight now refuses tracked/unignored
local-only files before archive/stage and permits only reviewed local-only
staged removals. Actual preview and21 path/public tests/syntax/content/
310refs/mirrors/24safe-clean PASS;18-case isolated Git regression PASS.
No actual commit/push or product rebuild; details in HANDOFF.

2026-10-19 publication/report operation: six-repo/eight-delivery catalog adds
F24 device Transfer/Boot/App;15 isolated Git checks and4path tests PASS.
Real read-only preview refuses missing F24 .git before cleanup; no actual
init/commit/push. Canonical report steps describe automatic HTML/JSON,
Open Last Report, history and separate CAN/Misc write/RUN outcomes; gallery
completion steps link to the same owner.43focused tests/content/310refs/
mirrors/24safe-clean PASS;5PDFs84pages36images visual QA and web1366/390 PASS.
Existing EXE/firmware hashes unchanged; no hardware writes or deployment.

2026-10-19 F24 device normal update accepted: user report confirms1.0.3/4224B/
CRC/App confirmation/CAN_STOP and UART V1 counter0..26. Same normal workflow
as F13 device. Two public PNGs/16operation assets, all9PDF/MD review;4PDFs
refreshed and69pages visually checked.38focused tests/310refs/24safe-clean/
source mirrors/browser1366/390 PASS. High-rate/fault acceptance remains scoped;
no runtime rebuild, COM/Flash or publishing. Details and evidence in HANDOFF.

2026-10-19 device log/bitrate/IRQ: F24 App-only1.0.3 clean0errors/5existing
warnings,4224B/CRCCF1C72F8; Boot/Transfer/PC binaries unchanged. V1/V2 tag and
decimal counter execute in main. Canonical workshop adds complete device
250K/500K/1M steps, separate F13 timing headers, shared F24 preinclude and
Boot/App same-name ISR ownership.34focused tests,9artifact checks/C models,
sync/public/304refs/24safe-clean PASS. PDF24pages6diagrams and browser1366/390
verified. User's preceding normal500K update/CRC/App evidence is recorded
separately from unresolved post-update CAN errors. New App board/high-rate/
fault tests NOT RUN. No host/Tool rebuild, COM writes or publication.

2026-10-19 F24 Custom Transfer gallery: four user operation captures added to
the canonical MULTIBUS_TOOL_GUIDE/ISP chapter, explaining live device identity,
cached offline BIN and separate host/device counters. Public lossless copies
mask local paths; originals retain hashes in ignored private archive.
MD/4assets/MultiBus PDF15pages8images synchronized, browser1366/390 and32 focused
source/media/public/PDF checks PASS; actual evidence/limits in HANDOFF. These
captures establish normal discovery/transfer only, not fault or Flash tests.
No runtime changes, compiler invocation, COM writes or Git/web publication.

2026-10-19 F24 device Transfer diagnostics: main-loop raw Classical error
capture/clear feeds GET_STATS and250ms UART trace; startup stage/result/raw
statuses/filter/P12 output are visible. Transfer clean build and trace1/0
production-code models PASS; Boot/App clean rebuilds retain exact hashes and
Boot spare514. Nine artifact checks, production metadata/Tool models and20
source/sync/public/PDF regressions PASS. MD/ISP/MultiBus PDF synchronized;
actual evidence and hardware/browser NOT RUN boundaries are in HANDOFF.
PC executable, host firmware and wire format unchanged; no publication.

2026-10-19 operation media: direct packaged App MOT download, Boot-only/combined
image usage and erase scope clarified. Fourteen redacted operation captures
are canonical in the shared guide and synchronized to ISP; originals preserved
in ignored private archive with identical hashes and raw-root publication
exclusion. ENTER inactivity and fresh-mode rejection match source.31 focused
tests, three PDF render/visual reviews, source/public/refs/clean/browser checks
and publication PLAN ONLY PASS. Tool/firmware unchanged; no COM/flash/push or
new physical timeout/fault acceptance. Exact owners/evidence in HANDOFF.

2026-10-19 classic console: five thick system-gray/white MFC work groups and
four semantic metadata groups replace general pastel panels. Auto scroll down
defaults on, persists in INI, preserves reading viewport/selection when off
and returns to bottom on re-enable; recording/protocols unchanged.0092 app
builds, startup/close/default1/prepared0 persistence, native Debug/Release
suites including15000-line RichEdit histories,49 focused Python tests and
MD/web/PDF/public/clean checks PASS. Actual main-window visual/TAB acceptance
NOT RUN(native pipe unavailable); no physical tests or publishing performed.
See HANDOFF/SELF_CHECK for files, hash, evidence and verification boundaries.

2026-10-19 completion UX: verified update/RUN emits a typed dialog and persistent
result/next-step/footer. CAN App-confirmation precedes CAN_STOP/COM retention;
STOP failure is an explicit cleanup warning with CAN state retained. Misc
write remains Boot and RUN is explicit. Discovery covers Boot/App; ENTER is
for discovered App only.0091 Debug/Release builds/process smoke, native suites,
45 focused tests, GDI widths, MD/web/PDF/refs/clean checks PASS. Native visual
and physical update acceptance NOT RUN; exact evidence in HANDOFF/SELF_CHECK.

2026-10-19 Tool operation reference: all20 shared metadata/image captions,
normal/error/action tables, settings, live/offline and CAN/Misc/I2C boundaries
added to the single-owner UPDATE_EVIDENCE block. Operation navigation and README
link there. PDF table/heading reader corrected;32 focused tests, public/sync/
resource/safe-clean checks and web/PDF rendering/visual review PASS. Runtime
EXE/firmware unchanged; no commit/push or hardware action. See HANDOFF/SELF_CHECK.

2026-10-18 Tool experience: worker-owned JSON/HTML update reports, Preflight
snapshot, exact-error guidance and local PnP selection hints implemented;
Metadata editor excluded. Tool0090 Debug/Release builds, isolated startup/close,
native full suites and29 focused Python tests PASS. HTML offline/browser,
MD/ISP/PDF rendering/visual, web/refs/clean checks PASS; one F13 map-dependent
budget audit NOT RUN after delivery pruning. Evidence in HANDOFF/SELF_CHECK.
Native Computer Use pipe unavailable and no COM ports: UI visual and physical
speed/unplug/power acceptance remain distinct NOT RUN gates.

2026-10-17 F24 host metadata: common bridge_image_metadata.c/.h is independent
of command/Flash engine, with project image_metadata_config/profile and
boot_app_handoff headers. App release/IRQ authoring uses C headers, with linked
identity and full vector packaging guards. Host Boot/App clean builds and
Tool0089 release/smoke PASS; native Debug/Release, strict C90/reference cases
and real App A/B with unchanged Boot PASS. Boot remains20KiB/App5000 with189
bytes spare. Host MD/local web/PDF synchronized; see HANDOFF/SELF_CHECK for
actual evidence. Physical verification NOT RUN. Device firmware untouched.

2026-10-17 device runtime parser: image_metadata.c/.h owns portable C90
decoding/validation, image_metadata_profile.c supplies per-project contract,
CAN staging remains in its adapter. Both standalone Boot/App pairs compile
the C units separately. Final builds,12 strict C90 combinations,54 focused
regressions, native/fault models and both App A/B flows PASS. Boot spare
F13894/F24514 retains512guard. MD/local web/PDF integration and actual
verification details are recorded at the top of HANDOFF/SELF_CHECK.
Physical tests NOT RUN; F24 host and Tool runtime are outside this change.

2026-10-17 device header ownership: image_metadata_config.h owns neutral IMAGE_*
format/MCU/Flash/policy/Boot identity; can_boot_config.h owns CAN, and
boot_app_handoff.h owns RAM/owner. Both golden generators read these three
explicit sources; App release/IRQ headers remain App-local. Clean Boot/App
builds and byte-identical deliveries, independent App A/B builds, negative
header cases, native models and MD/web/PDF checks are recorded at the top of
HANDOFF/SELF_CHECK. No Tool runtime/host changes or hardware claim.

2026-10-17 publication entry: root github_push.bat, explicit Execute/Yes and
read-only PS preview; fixed F13 Boot/App/Tool catalog includes F24 CAN host
deliveries. Archive non-delivery DefaultBuild files under ignored tmp before
stage/commit/push. Exact index, ancestry, staged-work and path/reparse guards;
no force, tags, submodule or web publication. Eleven isolated local Git scenarios
and safe-clean audit pass. See docs/GITHUB_PUBLISH_zh-TW.md and run transcripts
for actual remote results. No product rebuild/bump for this workflow change.

2026-10-17 device version policy: APP_IMAGE_VERSION_CONTROL is the sole switch,
default0, in the six-field C header; policy3/metadata0x74. Valid installed App
controls normal ordering; no persisted floor. Independent force upgrade old
version firmware uses public3036 confirmation and command-local FORCE_START0E
only. Preserve all other validation gates, fresh live-worker check and no
password persistence. Magic is CAN+NUL, unrelated to controller port number.
Boot/App versions are independent. Current verification supersedes the earlier
proposal below:63 regressions/native Debug-Release/A-B builds PASS, Boot spare
F13890/F24514. MD/web/five PDFs synchronized; hardware/UI visual NOT RUN.

2026-10-17 device compile summaries: both C-header inputs have field/IRQ
comments and identical local PowerShell helpers emit app_build_report.html
after validated packaging. Actual BIN metadata/CRC/hash/all256bytes/64slots;
Tool operation JSON remains a separate runtime record. Both App clean builds
retain original BIN/Boot hashes. Verification at top of HANDOFF/SELF_CHECK.
Version-policy Boot enforcement awaits agreement; latest proposal is one
metadata switch and read-only Tool display, avoiding two independent switches.
user chooses valid-installed-App checks only, no persistent Flash floor.

2026-10-17 update observability: CAN automatic App confirmation, explicit Misc
RUN reconnect, shared metadata differences, stage progress, independent Cancel,
phase/counter reports, generic host telemetry/stay diagnostics and F13 read-only
VERIFY/detail statuses. F24 device generation/delivery names align with F13.
Current implementation/verification: see top of HANDOFF and SELF_CHECK.
No boards available; all physical high-speed/unplug/power/fault gates remain
NOT RUN. Do not freeze a hardware acceptance claim from software models.

2026-10-16 public-guide organization / F24 device PowerShell workflow:
single-owner web topics with jump links, current-operation wording, device-only
JSON/vector/porting comparisons and exact IRQ/address sources. Seven PDFs
regenerated/rendered/visually checked; clickable URL/chapter navigation verified.
F24 device prepare/package/test require PowerShell5.1/.NET, not Python.
Python-free PATH clean builds all3 PASS; ownerAA/55, Boot spare514, App BIN
unchanged. Nine PS checks + C/PC models PASS;37 focused regressions, nine
canonical checks,194 refs/anchors/256 byte rows and24 safe-clean scripts PASS.
No frozen Tool/host/F13 delivery-byte changes; no device writes/publication.
Physical F24 CAN/RFD/IRQ/fault and browser/DPI checks remain NOT RUN.

Historical status slices below describe their own verification points.

2026-10-16 device JSON authoring/workshop: F13/F24 CAN devices both use strict
app_image.json + app_irq_handlers.json; generators own metadata INC/identity/
vectors, with no manual INC fallback. Reject stale linked metadata/IRQ tables.
App clean rebuilds retain original BIN size/CRC/hash; Boot/host/Tool unchanged.
Ten-step teaching, byte/output/error tables, three flows and four PDFs updated;
both local web guides synchronized. Seven workshop +25 documentation tests,
F13 generator-only, F24 seven artifact/IRQ tests and safe C/PC models PASS.
Nine canonical web checks,198 local refs and24 safe-clean audit PASS. Full F13
Boot map/clnk audit unavailable; no hardware/COM writes or site publication.

2026-10-15 F24 CAN device: three independent Transfer/Boot/App projects use
4MHz target board,Classical500K,targetF24B/F2V1. Boot stays8KiB,App2000,
minimum512-byte spare and protected final1KiB. Mandatory map/RFD RAM/vector/
metadata/CRC packaging, seven artifact/IRQ stack tests and production C RAM
fault / production PC parser tests PASS. Host/Tool/F13 binaries unchanged.
Design MD and both web guides synchronized. No device writes or publication;
CAN register-level model and all F24 device hardware acceptance remain open.

2026-10-15 cross-layer CAN teaching: canonical CAN_UPDATE_WALKTHROUGH MD/PDF,
15 tables/9 Mermaid diagrams/15 source links map UI/BIN validation, F24 generic
UART-CAN transport and F13 RX/metadata/RAM/Flash/END. Worked wire/address example,
offline/live distinctions and success-evidence matrix; no protocol/runtime change.
Bounded canonical sync to ISP web, links from boot web/reader/public resources;
24 documentation tests and7 canonical checks PASS, web198refs/loopback7routes,
15source URLs200, PDF20/14pages rendered/visually reviewed, clean audit24 PASS.
Six delivery hashes unchanged; no product build/bump/device write/publish.
Browser/DPI/hardware acceptance NOT RUN; previous map-audit gap unchanged.

2026-10-15 Flash/metadata/CAN byte reference: new canonical MD/PDF and boot web
section with512 concrete Flash addresses (256bytes for each board), real-BIN
values, field-source maps, CRC scopes and all CAN request/response/raw layouts.
CAN3 and BUR1 layouts remain distinct; F24 host is not future F24 CAN device.
Generator/check-only and bounded web sync helper added, no runtime changes.
Reference6 + guide/public13 tests PASS, including C90 sample compile; actual
18 GitHub source links200. Canonical6/web198refs/byte rows PASS. PDFs28/14 pages
rendered/reviewed. Firmware/Tool delivery bytes unchanged; no device writes,
product rebuild/version bump or Git/site publication. Browser/DPI NOT RUN.

2026-10-15 CAN role labels: F24 (MCU CAN host), F13 (MCU CAN device) on both
web slides, download/flow/porting/IRQ/metadata entries and root navigation.
Future F24 (MCU CAN device) is a separate project; no device development added.
Host self-update remains Misc/UART; legacy UART/I2C roles are not mass-relabelled.
Canonical5/web198 refs and13 guide/public/PDF tests PASS; ISP11 Mermaid render.
PDFs30/10/14/10 pages visually reviewed; binaries/URLs unchanged. No runtime
build, COM/device write, Git push or browser/DPI validation in this wording task.

2026-10-15 public download/flow index and checkout portability: six verified
download URLs, F13 Transfer/Boot/App repositories and six stable workflow links
are canonical in MULTIBUS_TOOL_GUIDE PUBLIC_RESOURCES, synchronized to both
web guides. Corrected cross-repository README links and stale metadata text.
Normal solution builds exclude integration tests; explicit F13 tests use
sibling checkouts or configured paths, not personal workspace includes.
Tool and test Debug/Release builds0warnings/errors; both actual-image test
suites and neutral relocated sibling build/run PASS. Guide/public/path tests15,
canonical sync5, web198refs, HTTP routes and safe-clean24 PASS. Four PDFs
30/10/13/10 pages,20/5/7/2 diagrams rendered/reviewed. PDF binary Git attributes
preserve bytes. Six delivery hashes unchanged; no version bump/firmware rebuild.
Full Python discovery35 ran:34PASS/1optionalSKIP, plus F13 map-dependent class
setupERROR because maps were cleaned. No new COM/flash/browser/DPI acceptance.
Unused generated assets/reports/private IDE state moved to recoverable archive;
no permanent deletion, staging, commit, push or site deployment performed.
Detailed evidence and restore location: tmp/internal/public_portability/VERIFICATION.md.

2026-10-15 public web copy: canonical linked RL78 ISP Tool entry; remove draft/
publication and delivery-build/snapshot wording from navigation/intros and
shared operating blocks. Preserve protocol/metadata/IRQ and unverified limits;
exact versions/hashes/performance stay in delivery records. verify_local now
checks product identity/evergreen copy, not a current build-number string.
Four new future-version/link/copy/contract tests PASS; no runtime/binary change.
Final public/guide/porting/PDF10/canonical4/web197/HTTP7+navigation/sources/
safe clean24 PASS; PDFs30/10/11/9 generated/rendered/visually reviewed.
No product rebuild/COM/Git/public deployment; browser/DPI acceptance NOT RUN.

2026-10-15 first engineering delivery: user182253 verifies Tool0082/F24App1.5.0/
F13Boot1.5.0,COM6/115200/CAN500K,19244B/CA0DF55C,301 ordered ACKs,
10963ms including metadata/END,1755B/s, no reported retries/overflow/CAN errors.
Keep measured binaries and F24Boot1.2.0 unchanged; no rebuild/version bump.
docs/FIRST_DELIVERY_20261015_zh-TW.md owns upload list/SHA-256/install/limits.
Single v5 result supersedes only the earlier speed/normal-write pending gate;
post-reset App evidence, real fault/IRQ/native UI/high-baud gates stay OPEN.
No Git/release/site publishing authorized or performed by this preparation.
Final test-only Debug/Release builds/suites, six hashes/HEX+BIN byte/CRC checks,
guide6/canonical4/web196/clean24 PASS; PDFs30/10/11 rendered/visually reviewed.
Full Python24PASS/1optionalSKIP/1setupERROR and map/IRQ audit not rerun because
F13 linker maps were cleaned; prior clean-build results and artifact hashes
remain the evidence. Do not silently turn that verification gap into PASS.

2026-10-15 CAN v5/Tool0082:64-byte RAM blocks,8-byte indexed raw frames and one
COMMIT ACK;19224B needs301 block transactions instead of4806 DATA transactions.
Generic bridge22/bit12/F24App1.5.0, F13pairedBoot/App1.5.0 required; F24Boot1.2.0
unchanged, Misc115200. F13 remains8KiB/App2000/F1V1 with530B spare (512guard).
Metadata-before-write, per-block/full-image CRC and no-rewrite replay retained.
Real-source/model fault/tail/HAL tests, Debug/Release0warnings/errors, CC-RL
and artifact guards PASS; Python29 with1explicit optional fixture-build skip.
User0080 log171150 is77/79s old-v4 evidence, not v5 acceptance; its incomplete
trace prompted explicit queue-drop warnings. MD/web/PDF30/10/11/9 sync/visual,
canonical4/web196/current0082/clean24 PASS. Native GUI pipe error2; physical v5
speed, high baud, bus-off/power-loss/IRQ still OPEN. No device writes/publishing.
Detailed file/evidence ledger: tmp/internal/can_block_v5/VERIFICATION.md.

2026-10-15 PC0080: first-byte SerialPort receive removes short-reply20ms wait;
idle waits bounded by caller deadline/50ms, zero-timeout drain, timeout cache,
no Sleep polling. Parser/CRC/retry/cancel/write/baud defaults and firmware/wire
unchanged. Real SerialPort Win32 model tests were RED then GREEN, including
max128/partial/coalesced/CRC recovery, errors/wrap/deadlines/cancel/4806 exchanges.
Release/Debug0warnings/errors and actual-image suites PASS; delivered0080,
INI/F13/F24 images unchanged. User0079 evidence confirms one19224B F13 CAN
update,4806DATA/about142s and V2 App launch, not a full fault-matrix acceptance.
MD/web/PDF30/10/11pages synced/visual checked; startup3/route2/artifact3/guide2/
porting4/sync4/web196/clean24 PASS. Native GUI blocked by pipe/os error2;
0080 physical speed/high-baud A/B still OPEN. No COM/driver/flash/publication.
Evidence: tmp/internal/serial_latency/VERIFICATION.md.

2026-10-15 PC0079/F24 App1.4.1:154213 log INFO50 validApp then57BAD_COMMAND
is missing outer App READ_METADATA route, not a failed CRC/programming result.
Added route without readback bypass. Fixed startup EN_CHANGE before INI path;
loading guard starts true, no empty-path writes, real failures still visible.
App clean build40944B/CRC9F32E2B8,0errors/2existing license warnings; Boot
MTPJ/vector/MOT and INI hashes unchanged. PC Release/Debug0warnings/errors,
both final actual-image suites, new App-handler/reopen tests, route2/startup3
source tests PASS. MD/web/PDF30/10/11/9pages synced/reviewed; guide2/porting4/
artifact3/layout4/sync4/web195/current0079/clean24 PASS. SW1/P32 reset recovery
to unchanged BOOT115200, Misc reload1.4.1/update/explicit RUN is documented;
physical recovery/reconnect and native startup remain OPEN, no COM/flash.
Optional detailed evidence: tmp/internal/app_readback_fix/VERIFICATION.md.

2026-10-15 PC0078: fixes progress caption wrapping/clipping at footer; actual
font width reserves100%, controls stay in the footer and do not overlap.
Only operation-name changes re-layout, not each packet. Original145px defect
reproduced by GDI;8labels/72size-font combinations/360percent captions PASS.
Release/Debug one-click builds and actual-image full suites PASS,0errors/
warnings;0078 EXE delivered. INI/F13/F24 artifacts unchanged, firmware and
update contracts untouched. MD/web/PDF sync, guide/layout tests and clean24
PASS. Native GUI startup/screenshots/actual desktop DPI/update remain NOT RUN
(native pipe os error2); no COM/flash/publishing. Optional internal evidence:
tmp/internal/progress_footer/VERIFICATION.md.

2026-10-15 F24 consolidation: Bridge_boot/Bridge_app are the only products;
f24_board owns shared board/drivers/SC without an MTPJ. App runtime is local;
portable I2C staging is common. Prepare updates each existing project rather
than cloning the retired standalone project; preserves options/hooks/JSON.
Only source boundaries/paths changed; Boot MOT/App BIN/Tool EXE/INI/0077 identical.
Both clean F24 builds/guards,5real App fixtures,4structure tests, Debug/Release
PC/full suites, F13 audit, guide2/porting4/artifact3, sync4/web195/Mermaid11+9/
clean24 PASS. PDFs10+9pages reviewed. SC GUI regeneration/native GUI/physical
flashing/recovery remain OPEN; no traffic/publication. Details are disposable:
tmp/internal/F24_TWO_PROJECTS.md. Initial Boot-only installation followed by
Misc115200 App update is documented; Misc never replaces Boot.

2026-10-15 folder organization: docs=current guides/specifications;
build=current delivery/settings/user runtime logs; tmp/internal=disposable
verification/plans/old tools/compiler/test/log output. Historical evidence
references below are optional after tmp removal, not build/runtime inputs.
No protocol, UI feature, firmware or product version change.
Verified Debug entry/Release direct/test builds0warnings/0errors, full PC
Release/Debug suites,5 real F24 fixtures, layout3/porting4/PDF2 tests, sync4/
web195/clean24. No GUI launch/actual clean deletion/board traffic; current
EXE/INI hashes preserved. Optional details: tmp/internal/CLEANUP.md.

2026-10-15 firmware porting documentation: unified RL78_BOOT_APP_PORTING_GUIDE
MD/PDF, canonical local web block plus ISP/navigation entry. F13/F24 current
layout/IRQ/RAM/CS+ engineering steps and custom-flow boundaries; old metadata/
callback wording corrected.4 new tests, existing2 PDF tests, artifacts/IRQ
model/generator checks, four sync checks,195web refs/7HTTP/11+9charts and
reviewed9-page PDF/clean24 PASS. No code/firmware rebuild or binary changes;
Tool stays0077, physical/browser acceptance still OPEN. Full evidence:
tmp/internal/docs/RL78_PORTING_GUIDE_VERIFICATION.md.

2026-10-15 Host Link0077: APP115200/460800/921600, feature11/05..07,
old-speed ACK drain/new-rate confirm/2s rollback, selected-COM recovery and
read-only sequence collision handling. Device UART baud is a separate
compile-time policy2 field112; its tab/backend/programming remains deferred.
PC Release/Debug tests, F24 app1.4.0 and monolithic build47 compile/mock pass;
BOOT MOT and F13 HEX/BIN unchanged. Physical high-speed/native UI remain OPEN.
See docs/HOST_LINK_BAUD_zh-TW.md and HOST_LINK_BAUD_VERIFICATION.md.

2026-10-15 common policy0076: metadata256/core128/reserved128 with UPD1
role/transport/IRQ ABI/minboot.18 shared UI fields and cross-TAB wrong-route
gates, worker checks, F13 RESET/stage/seal before START; BUR1contract3 before
BEGIN. F13 boot/app1.4.0 format6/protocol4; F24 boot1.2.0/app1.3.0; matching
initial boot/app install required, partitions/vector workflows unchanged.
Final PC Release/Debug/full actual-package tests, F24 clean builds and5 real
A/B tests/both PC fixture suites, F13 clean artifact/IRQ audit, real C metadata/
CRC/two768-cut models, clean audit24 and MD/web/PDF checks PASS. Hardware,
native UI and physical CRC timing remain OPEN. G16 new I2C boot/app explicitly
DEFERRED, programming remains blocked. See tmp/internal/docs/COMMON_METADATA_VERIFICATION.md.

2026-10-15 F24 per-app engineering flow: app-local IRQ/identity JSON,
pre-build generation, app-only -AppProject build/package, reference -Kind app
isolation. Five tests with real A/B IRQ/version builds and unchanged boot
hashes PASS; final reference clean builds0errors/2license warnings, same boot
MOT/reference size-CRC. PC0075/UART boot/BUR1 remain unchanged. Debug build,
both full suites, F13 artifacts, clean24, web194/7+7charts and shared PDF7pages
visual/parser QA PASS. Physical A/B IRQ/update/debug/power-loss still pending;
no GUI change or native test this turn. tmp/internal/docs/F24_APP_LOCAL_IRQ_VERIFICATION.md.

2026-10-15 fixed F13 IRQ follow-up0075: full boot-owned normal vectors,
app-local mapping/identity, normal RETI for all app ISRs including INTTRJ0.
Format5 requires one initial matching boot1.3.0/app install; future compatible
app IRQ/version additions need only app updates. ROM margin568/guard512,
F13 clean builds/artifacts/7808 stack-model cases, PC Release/Debug full suites,
clean24, web194/current canonical blocks and final40-page PDF QA PASS.
Native UI unavailable after retry/reset; real-board IRQ/debug/update/power-loss
still MANUAL NOT RUN. No F24 firmware changes, flashing or publication.
Files/results/reproduction: tmp/internal/docs/F13_FIXED_IRQ_VERIFICATION.md.

2026-10-15 left-only TAB follow-up0074: CAN/I2C/Misc confined to left settings;
independent shared image/metadata and log headings, original single resources
preserved. Full F13 boot decision/ENTER/timer/error/reset tables and diagrams
are canonical in guide2.2 and identically synchronized to both web guides.
PC Release/Debug one-click/full suites, unchanged F13 artifacts, PDF2tests/
38-page visual QA, web194refs/5+5Mermaid and clean audit24 PASS. Native startup/
DPI/TAB checks NOT RUN (pipe unavailable after reset); HTTP preview blocked.
No MCU code/rebuild/board actions/publication. Evidence and exact file list:
tmp/internal/docs/LEFT_TABS_BOOT_DECISION_VERIFICATION.md.

2026-10-15 Host128/metadata256 follow-up:0073 requires128-byte hosts only;
frame142, metadata256/core96/reserved160FF, bridge contract2 with four64-byte
STAGE/READ pages and CRC-gated BEGIN. F13 format4/protocol3. Final PC Release/
Debug builds/full actual-package suites, F24 clean builds, F13 artifact audit,
clean audit24, local web194refs/0073 and three PDF visual reviews PASS. F13
boot has5warnings (license/vector/record), app3license warnings; F24 each2license
warnings. Matching new boot/app initial installation required. Native UI and
all physical update/debug/power-loss gates remain OPEN; G16 programming stays
BLOCKED. See tmp/internal/docs/HOST128_METADATA256_VERIFICATION.md for evidence and files.

2026-10-15 delivery-policy follow-up: functional changes now require affected
MD/PDF plus local web instructions/status/screenshot synchronization. User
authorizes automatic normal close of this project's tool for builds. Full-path
guard and15second bounded shutdown replace name-only forced termination;
mock tests/Debug one-click/clean audit PASS. Local web catches up to0072,
194 references/version check and ISP3/Boot3 Mermaid rendering PASS. Actual
process-close/HTTP/browser/UI gates NOT RUN; no app or firmware rebuild/change,
no publication or board traffic. See tmp/internal/docs/BUILD_WEB_SYNC_VERIFICATION.md.

2026-10-15 shared-console follow-up:0072 software implements1366x768 no-parent-
scroll console, shared image/metadata/log/progress and Misc HOST APP UART path.
F24 split boot/app20KiB/5000 use shared SC, RAM RFD/IRQ/stack guards and dynamic
BUR1 CRC32 packaging. PC Release/Debug/full mocks and F24 clean builds PASS;
final app37644B CRC88FDB8C6; F13 artifacts/clean audit24/PDF5pages2diagrams PASS.
User closed the delivery lock; canonical EXE updated. Native UI/DPI/ghosting and
all physical boot/app/debug/flash/power-loss gates remain OPEN. See
tmp/internal/docs/CONSOLE_BRIDGE_VERIFICATION.md; no programming/publication was performed.

2026-10-15 repaint follow-up: user reported TAB/vertical-scroll ghosting.
Tool0.1.0.0071 centralizes erase/all-child repaint after final layout/clamp;
Release/Debug builds and full suites PASS, clean audit PASS24. Actual TAB/scroll
visual acceptance remains open: desktop native pipe unavailable. No firmware
or protocol change; this is not a new hardware-release PASS.

2026-10-15 UI/web follow-up: tool0.1.0.0070 final Release/Debug and full tests
PASS; five live/offline role cards, brighter palette and provenance tests.
Local CAN/ISP web drafts, benchmark assets, HTTP/link/Mermaid checks and
reviewed identity/multi-bus PDFs prepared. No publication, board firmware
change or new physical PASS. UI screenshots/startup/DPI/browser visual QA
remain open because desktop/browser surfaces are unavailable; see SELF_CHECK.

- `[ ]` Not started
- `[~]` In progress
- `[x]` Completed and verified
- `[!]` Blocked; record the reason directly below the item

An item is not complete until its verification result and relevant log or build
artifact have been recorded.

Multi-bus software integration now builds and passes host-side tests. CAN/I2C
tabs/status, capability-based ports, generic staged I2C/F24 backend and G16
inquiry/signature/geometry/CRC16 checks are implemented. The supplied G16 slave
lacks active READ/auth and safe boot CRC/recovery gates: programming remains
BLOCKED rather than counting generic transport as a complete upgrade workflow.
Active source/F24 project moved to simple_renesas_isp_tool; only the locked
original schematic remains at the old path. Startup/DPI and physical I2C/CAN
regression remain open. See SELF_CHECK and HANDOFF for actual evidence.

Software-completion snapshot (2026-10-14): previous CAN-scope F24/F13/PC
implementation, host-side mock tests, identity, filter/mode UI, diagnostic,
reset/reconnect, transaction-queue, CRC post-build, and artifact-checker work is
complete. Remaining unchecked items require physical-board measurements or
fault injection, user/product acceptance based on those measurements, the
explicitly deferred RA2L1/RA6 phase, or final hardware-evidence review,
logs, and screenshots. The 16 source-verified Mermaid diagrams and visually
inspected Traditional Chinese PDF exist, but are not release-behavior verified
until the physical gates close. The complete checklist is not a release PASS
until those gates are closed.

## Confirmed design decisions

- [x] Use the current RL78/F24 Smart Configurator project and its selected MCU
      and peripheral configuration as the hardware baseline.
- [x] Use P10 for CAN TX, P11 for CAN RX, and P12 for CAN transceiver STB.
- [x] Use P15 for UART TX and P16 for UART RX to the USB-UART interface.
- [x] Implement IICA0 P62/P63 generic100kHz backend, bounded staging and PC
      integration; clean build and engine/client/register mocks PASS.
- [ ] Verify physical I2C timings, repeated-start, NACK and recovery.
- [!] G16 programming requires a corrected slave contract; no auth/read-back
      or safe boot CRC/recovery in the supplied active implementation.
- [x] Use SW1 on P32/INTP7 and SW2 on P70/INTP8.
- [x] Use LED1/LED2/LED3 on P80/P81/P82.
- [x] Use 500 kbit/s as the Classical CAN nominal bitrate.
- [x] Use 500 kbit/s nominal and 2 Mbit/s data bitrate for CAN FD.
- [x] Change all RL78/F13 Classical CAN projects from 250 kbit/s to
      500 kbit/s.
- [x] Do not provide an accept-all CAN receive-filter mode.
- [x] Reserve P15/P16 for binary Host Link traffic. Do not link or call
      `printf`/`putchar` in the F24 host; human-readable diagnostics belong in
      the PC tool. Interrupt handlers only capture state/data and return.
- [x] Keep target-specific transfer and firmware-update commands in the PC
      tool. Host-board firmware remains a generic CAN/CAN FD transport.
- [x] Write all new hand-maintained embedded C as strict C90 with declarations
      at the beginning of each block and English source comments.
- [x] Keep application logic outside generated files except inside preserved
      Smart Configurator user-code sections.

## Phase 0 - Freeze interfaces and acceptance criteria

- [x] Define Host Link Protocol v2 while retaining the proven UART frame shell:
      `55 AA + version/type/seq/cmd/status/length + header CRC16 + payload +
      payload CRC16`.
- [x] Add a strict maximum payload length suitable for a 64-byte CAN FD frame
      plus metadata; reject an oversized length before allocating or buffering.
- [x] Define request, response, and asynchronous event frame types.
- [x] Define protocol status values for bad command, bad argument, CRC error,
      sequence error, busy, timeout, CAN error, overflow, and internal error.
- [x] Define these generic host commands:
  - [x] `GET_HOST_INFO`
  - [x] `GET_CAPABILITIES`
  - [x] `SET_CAN_CONFIG`
  - [x] `SET_CAN_FILTER`
  - [x] `CAN_START`
  - [x] `CAN_STOP`
  - [x] `CAN_TX`
  - [x] `CAN_TX_WAIT_RX`
  - [x] `CAN_RX_EVENT`
  - [x] `GET_STATS`
  - [x] `CLEAR_STATS`
  - [x] `RESET_HOST`
- [x] Define a duplicate-request/replay rule so a retried UART request cannot
      accidentally transmit the same CAN command twice.
- [x] Define an explicit PC/host session state machine instead of distributing
      connection and update state across unrelated Boolean flags.
- [x] Define Host Link version negotiation and reject an incompatible PC/host
      firmware pair before CAN is started.
- [x] Include supported CAN modes, timing presets, filter capabilities, maximum
      frame/payload size, queue depth, and optional features in host capability
      negotiation.
- [x] Define host identity independently from target identity:
  - [x] Host board type: RL78/F24 now; RA2L1 and RA6 later.
  - [x] Host firmware version and Host Link Protocol version.
  - [x] Target family/device/profile: RL78/F1x, RL78/D1A, RL78/F2x, and the
        exact supported device identifier.
- [x] Define single-target bus behavior for the first release.
- [ ] After physical timing measurements at the supported UART rates, obtain
      user/product acceptance for the maximum F13 full-image update time.
- [x] Decide the first-release security policy: CRC-only integrity validation
      is accepted; digital signatures/authentication and anti-rollback are
      explicitly out of scope. The boot loader must validate manifest and full
      image CRC before every app jump and remain recoverable in boot mode.
- [x] Write the protocol definition before implementing either endpoint.

## Phase 1 - Establish clean build baselines

### PC tool

- [x] Record a clean x64 Release build of the copied PC project before major
      refactoring.
- [x] Record current compiler warnings and distinguish existing warnings from
      newly introduced warnings.

### RL78/F24 host firmware

- [x] Confirm the CS+ device is the MCU fitted to the current F24 host board.
- [x] Treat the current Smart Configurator settings as authoritative.
- [x] Regenerate driver code through Smart Configurator after configuration
      changes; do not hand-edit generated implementation sections.
- [x] Use the internal HOCO at 40 MHz for `fMAIN`/`fMP`/`fCLK`; keep X1 and PLL
      out of reset-to-main startup so Host Link cannot stall on oscillator or
      PLL lock before `main`.
- [x] Route F24 CRXD0/CTXD0 to P11/P10 in both `.scfg` and generated settings
      (`PIOR46=0`); do not use the P73/P72 alternate route.
- [ ] Reflash F24 firmware build 17 or later and verify CS+ reaches `main` and
      PC `Open + Probe Host` returns identity on the physical board.
- [x] Correct the stale `R_Config_INTC_INTP0_Start()` call to the configured
      INTP7/INTP8 interfaces.
- [x] Perform a clean build after regeneration; do not rely on the existing
      pre-regeneration `.obj`, `.map`, or `.abs` files.
- [x] Save build result, compiler version, code size, and RAM usage.

### RL78/F13 targets

- [x] Record clean builds for:
  - [x] `RL78_F13_CAN` (current Transfer reference project name)
  - [x] `RL78_F13_Boot_loader_CAN`
  - [x] `RL78_F13_Boot_loader_CAN_app`
- [x] Preserve the existing boot/app flash layout and shared-vector contract.

### Recorded baseline results (2026-10-12)

| Component | Command/configuration | Result |
| --- | --- | --- |
| PC tool | MSBuild, x64 Release | PASS, 0 errors, 0 warnings |
| F24 host | CS+ `/br`, CC-RL V1.15.01 | FAIL at link: stale `R_Config_INTC_INTP0_Start()` reference; 1 error, 3 warnings |
| F13 Transfer | CS+ `/br`, CC-RL V1.15.01 | PASS; program `0x41BF`, ROM data `0x055B`, RAM data `0x00B2`; 2 warnings |
| F13 boot | CS+ `/br`, CC-RL V1.15.01 | PASS; program `0x19E3`, ROM data `0x0362`, RAM data `0x092D`; 5 warnings |
| F13 app | CS+ `/br`, CC-RL V1.15.01 | PASS; program `0x3DA5`, ROM data `0x0497`, RAM data `0x00A2`; 3 warnings |

All firmware builds report that the paid CC-RL V1 license is unavailable and
the evaluation period has expired. The F13 projects also report automatic
project-tool migration from CC-RL V1.13.00 to V1.15.01. The app post-build step
successfully generated a 122,880-byte update image with target `0xF13B`, version
`0x00010000`, and CRC32 `0xC711A197`. The F24 baseline also reports that its
device changed and its generated I/O header must be updated. CS+ returned a zero
process exit code even for the failed F24 link, so verification must parse the
CS+ build summary rather than trust the process exit code alone.

Post-500-kbit/s F13 clean rebuilds (2026-10-12):

| Project | Result |
| --- | --- |
| F13 Transfer | PASS after deterministic protocol integration; program `0x4875`, ROM data `0x0463`, RAM data `0x00CC`; 2 warnings |
| F13 boot | PASS; program `0x19E1`, ROM data `0x0362`, RAM data `0x092D`; 5 warnings |
| F13 app | PASS; program `0x3DA3`, ROM data `0x0497`, RAM data `0x00A2`; 3 warnings |

The app post-build step regenerated the 122,880-byte image with target `0xF13B`,
version `0x00010000`, and CRC32 `0xDA8DB250`. The active `RSCANconf` XML sources
and generated headers all use prescaler 2 with 11/4 TSEG1/TSEG2 and 4-TQ SJW.
Physical CAN timing remains to be verified on hardware.

The F13 Transfer entry supersedes the earlier `0x41BD` intermediate build. Its
active XML/header pair now uses prescaler 2, exact request ID `0x710`, and no
accept-all rule. The unused `RSCANconf_default` reference remains unchanged and
is not part of the CS+ project.

Post-fix F24 clean rebuild: PASS with CC-RL V1.15.01, 0 errors and 2 license
warnings; program `0x59A8`, ROM data `0x09BC`, RAM data `0x00FA`. Schematic page
2 identifies the fitted MCU as `R7F124FLJ3AFB_C`, matching the CS+ project
device `R7F124FLJ3xFB`. The I/O header was regenerated from
`DR7F124FLJ.DVF` V1.10. After the latest Smart Configurator generation,
`.scfg`, `iodefine.h`, and active `src/smc_gen` headers identify
`R7F124FLJ3xFB`. The 2026-10-12 clean rebuild passes with Error 0 / Warning 2,
program `0x75E9`, ROM data `0x0675`, and RAM data `0x0812`. CS+ still prints a
device-change/I/O-header notice because inactive/history project metadata
retains FPJ5 records; this is not physical-board verification.

Latest F24 functional rebuild (2026-10-13): PASS with firmware identity
`1.0.0.5`, Error 0 / Warning 2, program `0x4CF2`, ROM data `0x0275`, and RAM
data `0x0919`. Source and linker-map checks find no `printf`, `_printf`, or
`_putchar`; removing the formatter reduced program usage by `0x2F34` bytes
from the preceding `0x7C26` build.

F24 startup-clock/CAN-routing rebuild (2026-10-14): PASS with firmware identity
`1.0.0.17`, Error 0 / Warning 2, program `0x4C70`, ROM data `0x0275`, and RAM
data `0x0919`. The `.scfg` and generated configuration select HOCO 40 MHz with
X1/PLL stopped and select P11/P10 for CRXD0/CTXD0. Physical host probe and CAN
waveform verification remain open.

## Phase 2 - Correct F24 board-level behavior

### Buttons

- [x] Configure SW1 as P32/INTP7 and SW2 as P70/INTP8.
- [x] Confirm the configured active edge and external/internal pull-up behavior
      against the board schematic.
- [x] Keep each button ISR limited to setting a flag or queueing an event.
- [x] Debounce and process button events in the main loop.
- [x] Do not assign a destructive update/reset action to a button until that
      behavior is explicitly specified.
- [ ] Verify both buttons independently on the physical board.
- [ ] Verify on the physical board that idle/aborted IICA0 and released P62/P63
      remain in a safe idle state without affecting bridge operation.

### LEDs

- [x] Configure LED1=P80, LED2=P81, and LED3=P82 as outputs.
- [x] Verify and document whether each LED is active-high or active-low.
- [x] Implement a non-blocking LED state machine with this initial mapping:

| State | LED behavior |
| --- | --- |
| Normal boot / idle | LED1 slow blink; LED2 and LED3 off |
| PC Host Link connected | LED1 steady on; LED2 and LED3 off |
| Transfer starting | LED2 turns on briefly before active transfer |
| Transfer in progress | LED1 steady on; LED2 fast blink; LED3 off |
| Transfer/protocol/CAN error | LED3 blink; error state has highest priority |

- [x] Drive LED timing from a timer tick processed in the main loop; do not use
      blocking delays.
- [x] Define the source-level error indication clear policy: reset enters Idle;
      a valid Host Link request enters Connected before dispatch; a CAN error
      returned by that request restores Error. `CLEAR_STATS` clears counters
      after its successful response without changing CAN state. Physical LED
      behavior remains open.
- [ ] Verify every LED state on the physical board.

### UART and diagnostics

- [x] Replace single-character demo processing with a bounded UART RX ring
      buffer and Host Link frame parser.
- [x] Remove the single-character demo/reset command and add a bounded 256-byte
      UART RX ring; the fixed-buffer Host Link v2 parser is now connected in
      the F24 main loop.
- [x] Add a 256-byte UART TX queue with main-loop transmission startup and ISR
      completion handoff.
- [x] Ensure binary protocol bytes are never mixed with ASCII `printf` output.
- [x] Remove every F24 `printf`/frame dump call and the `putchar` retarget from
      UART, CAN RX/TX, timer, button, initialization, and helper code. The clean
      build map contains no formatter or putchar symbol.
- [x] Convert ISR diagnostics into saturating counters/flags, expose them
      through binary `GET_STATS`, and decode them in the PC log/session summary;
      the dedicated Host Link UART never mixes in ASCII diagnostic output.
- [x] Track UART framing, parity, overrun, RX overflow, and protocol CRC errors.
- [x] Track UART framing, parity, hardware-overrun, RX-ring-overflow,
      TX-ring-overflow, TX-start, protocol header/payload CRC, invalid-version,
      invalid-type, and invalid-length errors.
- [x] Define the product watchdog policy. Neither the F24 host nor the F13
      boot/app firmware uses WDT. F24 option byte 0 is `0xEF`, F13 boot/app
      option byte 0 is `0xE9` (`WDTON=0`), and WDT refresh is disabled. Recovery
      uses bounded software timeouts, explicit reset, boot-mode retention, and
      image validation; watchdog-based recovery is intentionally out of scope.

## Phase 3 - Implement the generic F24 CAN host

- [x] Preserve the existing F24 CAN FD timing baseline:
  - [x] Classical CAN: 500 kbit/s nominal.
  - [x] CAN FD: 500 kbit/s nominal and 2 Mbit/s data.
- [x] Add runtime selection between Classical CAN and CAN FD.
- [x] Use only validated timing presets; do not accept arbitrary unsupported
      timing values from the PC.
- [x] Set P12/STB high in stopped/standby state and low before CAN start.
- [x] Replace fixed demonstration transmit frames with a generic frame API for
      Host Link `CAN_TX` and `CAN_TX_WAIT_RX`; legacy demo helpers remain
      unreachable from Host Link and can be removed in cleanup.
- [x] Preserve ID, IDE, RTR, FDF, BRS, DLC, data length, payload, timestamp, and
      controller status in Host Link messages.
- [x] Move received CAN frames from ISR context into a bounded queue.
- [x] Do not share a mutable CAN frame buffer between ISR and main-loop code
      without explicit ownership or a protected queue slot.
- [x] Define overflow behavior and expose lost-frame counters to the PC.
- [x] Add software reporting for bus-off, error-passive, warning,
      arbitration-loss, timeout, and TX failure. Smart Configurator channel and
      global error sources are enabled; callbacks feed saturating counters and
      asynchronous state/error events; the PC decodes raw sources, controller
      state, and cumulative arbitration-loss count. Physical injection remains
      a separate verification item.
- [x] Configure controller bus-off entry/recovery interrupts and log both
      transitions through asynchronous state events and `GET_STATS`. Physical
      recovery timing remains a separate verification item.
- [x] Replace the current unbounded CAN start/mode-transition waits with a
      bounded poll limit and explicit error return.
- [x] Replace unbounded CAN initialization, mode-transition, and TX-completion
      waits with bounded timeouts and explicit error states.
- [x] Ensure an unknown CAN error code cannot index beyond a text table or use
      an uninitialized diagnostic string.
- [x] Implement generic `CAN_TX_WAIT_RX` matching by ID/mask/frame flags and
      timeout without embedding F13 boot command knowledge.
- [x] Add a bounded generic transaction queue: one active plus two pending
      `CAN_TX`/`CAN_TX_WAIT_RX` requests, duplicate-safe sequence handling,
      `OVERFLOW` on saturation, and advertised capacity 3. F13 DATA remains
      deliberately serialized by target ACK.

### CAN receive filters

- [x] Use the current F13 CAN Upgrade metadata/bootstrap defaults:
  - [x] Host TX/request ID `0x700`.
  - [x] Host RX/response ID `0x701`.
  - [x] Standard data frame.
  - [x] RX mask `0x7FF` for an exact 11-bit match.
- [x] Define the F13 Transfer exact defaults: host TX/request `0x710`, host
      RX/response `0x711`, standard data, mask `0x7FF`.
- [x] Support GUI filter choices:
  - [x] `Metadata/default`
  - [x] `Custom standard ID/mask`
  - [x] `Custom extended ID/mask` when supported by the selected workflow;
        current F13 metadata uses standard IDs.
- [x] Remove the active accept-all rules, use the exact metadata/workflow
      default, and reject an enabled runtime filter with a zero mask.
- [x] Validate ID width, mask width, IDE, frame type, and selected CAN mode on
      both the PC and F24 generic CAN layers.
- [x] Reconfigure F24 filters only while CAN is stopped, then restart
      explicitly; the PC worker also rejects profile setup while CAN runs.

## Phase 4 - Rename and restructure the PC tool

- [x] Add a transport-independent Host Link v2 client/session layer with:
  - [x] Non-zero request sequences and sequence/command response matching.
  - [x] Byte-identical retry using the same sequence.
  - [x] Bounded asynchronous-event queue and diagnostic counters.
  - [x] `PING`, `GET_HOST_INFO`, and `GET_CAPABILITIES` parsing/validation.
  - [x] Fake-transport tests for host probing, retry, matching, event routing,
        background polling, bounded event overflow, one-shot host reset, and
        non-retry of a target NACK.
- [x] Make the Windows serial-port class implement the generic Host Link byte
      transport interface without changing the transitional UI workflow.
- [x] Add a generic PC CAN host client for configuration, one filter,
      start/stop, TX, and TX-wait-RX with wire-response validation.
- [x] Reject invalid PC-side CAN IDs, masks, zero-mask enabled filters, DLC/data
      lengths, RTR/FDF/BRS combinations, TX timestamps, and retry/timeouts
      before writing to UART.

- [x] Rename the solution, project, executable, resource file, window title,
      root namespace, version symbols, INI file, log filename prefix, and build
      scripts to `FirmwareUpgradeTool`.
- [x] Remove Nuvoton/NuLink-specific UI text, commands, flow logic, diagnostics,
      and documentation.
- [x] Preserve the native Windows MFC/C++17 implementation and static MFC
      deployment model.
- [x] Start the main window maximized.
- [x] Move implemented host/target connect and F13 Transfer operations off the
      UI thread; report progress through posted messages/events.
- [x] Route the F13 Boot firmware-update workflow through the same
      worker session.
- [x] Remove nested UI message pumping from the transfer loop so button and
      window events cannot re-enter an active update operation.
- [x] Define deterministic cancel behavior: stop queueing new work, finish or
      abort the active transaction safely, send target ABORT when applicable,
      stop CAN if required, and restore UI state once.
  - [x] Current host-probe/Custom Transfer cancel uses bounded 50-ms read slices;
        the worker alone stops CAN and closes COM.
  - [x] F13 Boot error/cancel after START performs bounded best-effort ABORT
        and CAN_STOP before queued COM teardown.
- [x] Separate PC code into these responsibilities:
  - [x] Serial/Host Link transport
  - [x] Generic CAN host client
  - [x] Target discovery and Custom Transfer target contract
  - [x] Firmware image parsing/validation through the format-3 metadata
        contract; current F13 start/capacity values are not generic tool
        limits, and routine UI refresh uses cached validation instead of
        recomputing image CRC.
  - [x] Custom Transfer workflow with the F13 project as its reference target
  - [x] UI and logging
- [x] Add an abstraction that allows future F24, RA2L1, and RA6 host firmware
      to expose the same Host Link Protocol.
- [x] Update saved settings without reading or writing the obsolete INI name.

## Phase 5 - Implement PC UI and logging requirements

- [x] Display COM number plus Windows friendly name; use canonical item data
      for serial opening, INI and reset/reconnect. Preserve refresh selection,
      numeric ordering and plain-name fallback. Catalog/unit tests pass.
- [ ] Visually verify wider COM/dropdown and responsive connection-button row
      at 100%/125% DPI, with real USB-UART adapter names and INI restore.
- [x] Add Host board information and firmware/protocol version display.
- [x] Complete `GET_HOST_INFO` successfully before enabling CAN configuration
      or target operations; a generic serial port opening is not sufficient.
- [x] Add discovered Custom Transfer device label, target ID, and protocol
      version display without restricting the diagnostic contract to F13.
- [x] Add application/boot state plus boot/app version/build, installed image
      size, MCU, layout and CAN display using the complete non-destructive CAN
      Upgrade v3 INFO page set.
- [x] Display loaded-BIN size, calculated/stored CRC and match result directly
      in the UI, together with offline app/MCU/layout/CAN manifest fields.
- [x] Persist `App Start Override` in the INI. `Auto` derives it from metadata;
      a numeric value is a safety assertion and must match image and device.
- [x] Keep `Transfer Test` and `Start Firmware Update` as separate actions.
      The update action has a fixed label and remains disabled while its nearby
      status lists missing host/workflow/CAN/discovery/boot/image/metadata
      prerequisites.
      A locally invalid image is a missing prerequisite, not an enabled action.
- [x] Name the diagnostic workflow `Custom Transfer`; show a neutral explanation
      that it does not use a firmware BIN, while `Load Update Image` remains the
      metadata-bearing CAN Upgrade input.
- [x] Render missing prerequisites, invalid metadata/CRC, and error log entries
      in red; render ready/valid status in green and neutral workflow guidance
      in blue.
- [x] Add CAN mode selection: `Classical CAN` or `CAN FD`.
- [x] Show 500K nominal and 2M data bitrate when CAN FD is selected.
- [x] Show 500K nominal bitrate for the F13 Classical CAN profiles.
- [x] Add the filter selections and validation defined in Phase 3.
- [x] Disable incompatible mode/workflow combinations; the current F24 host
      exposes Classical 250K/500K/1M and CAN FD at those nominal rates with
      fixed 2M data, while image/device metadata
      determines what the target accepts.
- [x] Keep current host probe, CAN setup, discovery, and Custom Transfer test
      stages logged on normal as well as error paths.
- [x] Add optional target-side Custom Transfer UART RX/TX frame logging outside
      ISR context, with the same ID/DLC/data/sequence/command/CRC comparison
      fields as the PC detailed CAN trace; document disabling it for stress and
      timing tests.
- [x] Log each layer with timestamp and direction:
  - [x] PC UART Host Link request/response/event sequence and command
  - [x] CAN mode, ID, flags, DLC, exact payload bytes, and timestamp
  - [x] Decoded target command/status/sequence/address where applicable
  - [x] Retry number, timeout, NAK, CRC failure, queue overflow, and CAN state
  - [x] Image filename, size, manifest fields, calculated CRC, and final result
- [x] Ensure normal UI logging does not reduce bridge throughput excessively;
      per-frame trace is an explicit, default-off diagnostic option.
- [x] Preserve log-to-file support and include tool build plus validated
      host/target identity information.
- [x] Add an exportable session summary containing configuration, target
      identity, image identity, frame/retry/error counts, elapsed time, and the
      final result.
- [x] Add an explicit `Reset Host` UI action. It stops CAN, sends
      side-effecting `RESET_HOST` exactly once, closes the old session, retries
      the same COM/baud for a bounded interval, and revalidates F24 identity and
      capabilities before re-enabling target operations.
- [x] Drain asynchronous Host Link events while the worker is idle and decode
      CAN RX, CAN state, host error, bus-off and arbitration-loss diagnostics.

## Phase 6 - Define and verify the F13 CAN Transfer profile

- [x] Replace the current generic echo demo with a deterministic test protocol.
- [x] Remove the current behavior that retransmits every received frame with
      the same CAN ID; it can create an echo loop when another node or adapter
      also echoes traffic.
- [x] Define request `0x710`, response `0x711`, and exact standard-data
      hardware filters with mask `0x7FF`.
- [x] Define and implement these non-destructive commands:
  - [x] `DEVICE_INFO`
  - [x] `PING`
  - [x] `ECHO`
  - [x] `PATTERN_TEST`
  - [x] `GET_STATS`
  - [x] `CLEAR_STATS`
- [x] Include logical target `0xF13B`, exact part `R5F10BMG`, protocol version,
      and command capabilities in paged `DEVICE_INFO` responses.
- [x] Change the F13 project CAN timing from 250 kbit/s to 500 kbit/s.
- [x] Update its documentation and remove stale 250 kbit/s statements.
- [x] Add a PC F13 Transfer codec with request encoding, strict response
      ID/type/sequence/command/CRC validation, error decoding, and complete
      multi-page device identity assembly.
- [x] Add a PC F13 Transfer client that drives discovery and every test command
      through generic `CAN_TX_WAIT_RX` with exact response matching and
      non-zero target sequences.
- [x] Add target-side byte-identical duplicate replay, changed-content sequence
      rejection, CRC-8 validation, bounded CAN initialization, and non-blocking
      TX timeout/abort handling.
- [x] Verify normal PC -> F24 -> F13 and F13 -> F24 -> PC payload integrity:
      Tool0063/F24 build40, `can_upgrade_tool_20261014_201510.log`, sequences
      1..22 match supplied F13 UART frames; RX/TX=22/22 and zero error counters.
      DEVICE_INFO/PING/ECHO/PATTERN_TEST/GET_STATS pass at 500K/115200.
- [ ] Verify physical CLEAR_STATS; the default UI Transfer Test does not send it.
- [ ] Verify standard/extended rejection rules, wrong ID, wrong DLC, CRC error,
      timeout, duplicate request, and queue-overflow behavior.
- [ ] Verify the target reports CAN warning, error-passive, bus-off, RX lost,
      and protocol-error counters without entering a permanent `while (1)`
      failure loop.

## Phase 7 - Integrate the F13 boot loader and application

- [x] Align boot/app board clocks with the verified Transfer X1 4-MHz /2 x16
      PLL32/CPU32/CAN16 baseline, without changing update IDs or flash/vector
      layout. Shared C90 board support handles bounded waits and inherited CAN
      reset; real-source clock/CAN mock tests pass.
- [ ] Physical boot/app 1.1.1 INFO/ENTER/update, power cycle and shared ISR:
      transfer benchmark success is not boot/app hardware verification.
- [x] Change both F13 boot and app CAN timing from 250 kbit/s to 500 kbit/s.
- [x] Rebuild both projects with identical request/response IDs and timing.
- [x] Introduce and document metadata-driven CAN Upgrade protocol v3:
  - [x] Request ID `0x700`
  - [x] Response ID `0x701`
  - [x] Logical target `0xF13B`
  - [x] Application start `0x2000`
  - [x] Maximum update capacity `0x1E000`
  - [x] Variable actual image size: 4-byte aligned, manifest size equals BIN
        length, CRC32 occupies the final four bytes
  - [x] 96-byte format-3 manifest carries MCU, flash layout, version/build,
        CAN modes/rates/IDs, minimum image size and exact image length.
  - [x] INFO pages `0x00..0x17` expose matching live device metadata.
  - [x] DATA uses a 24-bit byte offset instead of a 16-bit word sequence.
- [x] Add a non-destructive application-side identity response so connecting
      the tool does not have to force an immediate bootloader reset.
- [x] Provide bootloader build/version information through a compatible INFO
      response so field logs can identify the exact boot firmware.
- [x] Keep `ENTER` as an explicit update action, not a discovery side effect.
- [x] Align F13 E2 Lite debug/link settings with the proven SelfTest project:
  boot runs to `_boot_main`, app runs to `_main`, `-OCDTR` reserves the
  trace/self RAM area, and normal `.dataR/.bss` begins at `0xFE500` instead of
  `0xFE300`.
- [ ] On physical F13 hardware, confirm the boot project halts at `boot_main`
      and standalone app debug halts at `main` with normal Continue control.
- [x] Implement the PC CAN Upgrade v3 workflow for HELLO, INFO, ENTER, START,
      DATA, END, ABORT, metadata comparison, status decoding, timeout, and retry.
- [x] Validate the application manifest before sending START.
- [x] Refuse an image with the wrong magic, format, target, size, or CRC.
- [x] Accept valid 32/60/80 KiB and maximum-capacity images; START, DATA count,
      END sequence, progress, and CRC all use the actual validated length.
- [x] Respect the target's one-request-at-a-time requirement.
- [x] Replace or guard unbounded CAN initialization/mode waits in boot and app
      startup so a failed peripheral cannot trap the device without diagnosis.
- [x] Review FSL `BUSY` polling and recovery behavior. Do not abort a flash
      operation unsafely; define a documented reset/recovery policy if the FSL
      does not complete normally.
      Review confirms FSL internal automatic status mode, with disabled
      user-mode BUSY loops. Errors are returned and incomplete images are
      rejected by manifest/CRC. F13 boot/app option byte 0 is `0xE9`
      (`WDTON=0`), and WDT is not part of the F13 or F24 product design. Do not
      re-enable the disabled user-mode BUSY loops. Physical FSL/power-fault
      validation remains a separate gate below.
- [x] Handle the slower first DATA response at each flash erase boundary.
- [x] Never advance DATA sequence until the matching successful ACK is received.
- [x] Require a new discovery/START after target state, flash, size, image, or
      CRC failure; the PC does not guess or resume a sequence.
- [ ] Measure full-image update time at 115200, 460800, and 921600 baud.
- [x] Implement the generic F24 transaction queue without adding F13 flash
      semantics. Keep F13 update DATA serialized because each next address and
      sequence depends on the matching target ACK.
- [ ] Verify the updated app boots and all redirected shared interrupts operate.
- [x] Add an automated post-build boundary check:
  - [x] Boot/FSL sections end below `APP_FLASH_START`.
  - [x] App HEX/MOT contains no data below `APP_FLASH_START`.
  - [x] App code begins at the configured code start after jump/extension data.
  - [x] Manifest reports the map-derived variable image length and CRC occupies
        the final four bytes at the aligned linked end.
  - [x] Every enabled shared interrupt vector resolves to the expected app
        jump-table entry.
- [ ] Test power removal/reset before START, during block erase, during DATA,
      before END, and immediately after END.
- [ ] Confirm every interrupted/invalid image remains recoverable in boot mode
      and is never started as an application.
- [ ] Verify FSL initialization, erase, write, manifest, and CRC failures remain
      distinguishable in both target response and PC log.
- [x] Record the accepted first-release limitation: CRC32 detects corruption
      and interrupted transfer but does not authenticate firmware; signatures
      and anti-rollback are not implemented.

## Phase 8 - End-to-end verification

- [x] PC tool clean x64 Release build succeeds with no new warnings.
- [x] F24 clean rebuild succeeds after Smart Configurator regeneration; CS+
      still emits the recorded historical device-change notice.
- [x] F13 Transfer, boot, and app clean builds succeed at 500 kbit/s.
- [x] F13 boot/app clean maps contain `-OCDTR`, allocate normal RAM at or above
      `0xFE500`, and use the correct project-specific Run-to-main symbols.
- [ ] Verify F24 power-on/idle LED indication.
- [ ] Verify PC connect/disconnect indication.
- [ ] Verify transfer-start, transfer-active, success/idle, and error LEDs.
- [ ] Verify F13 Transfer using normal and boundary-size payloads.
- [ ] Verify a complete F13 firmware update through the F24 host.
- [ ] Power-cycle the F13 and confirm the new application starts.
- [ ] Test UART disconnect, CAN disconnect, wrong bitrate, wrong mode, wrong ID,
      wrong target image, interrupted update, timeout, and bus-off recovery.
- [x] Confirm no ISR performs formatted output or blocking waits by static
      inspection; physical ISR timing remains open.
- [ ] Confirm CAN hardware filters do not accept unrelated bus traffic.
- [x] Add host-independent PC unit tests for frame encode/decode, CRC, stream
      resynchronization, length limits, sequence matching, timeout/retry,
      manifest validation, and each target-profile state machine.
  - [x] Host Link v2 codec: CRC known vector, empty/96-byte round trip,
        97-byte rejection, wrong version/type/header CRC/payload CRC,
        split/concatenated input, noise/overlapping SOF resynchronization, and
        bounded parser overflow accounting.
  - [x] F13 Transfer profile: CRC-8 known vector, request validation/encoding,
        response ID/sequence/CRC/error validation, and complete identity-page
        assembly; fake Host Link tests also cover every target command workflow
        and exact CAN match construction.
  - [x] CAN Upgrade v3/F13 implementation: CRC32, full format-3 metadata
        validation, strict command/response framing, ACK offset/address/CRC checks,
        erase-boundary/END timeout selection, and invalid-image zero-traffic
        behavior.
  - [x] Full update state-machine timeout, duplicate, disconnect, cancellation,
        and ABORT fault injection.
- [x] Add a mock Host Link/CAN transport so normal, timeout, duplicate, corrupt,
      out-of-order, disconnect, and cancellation flows can be tested without
      hardware.
- [x] Inject a simulated target `FLASH_ERROR`/FSL failure in the update runner;
      verify DATA stops, END is not sent, and exactly one ABORT cleanup occurs.
- [x] Add mock coverage for one-shot `RESET_HOST`, background asynchronous event
      polling, bounded event queue overflow, and idle read timeout.
- [ ] Stress-test UART and CAN queues at their supported rates and verify
      counter behavior at the overflow boundary.
- [ ] Archive tool log, CAN trace, build logs, firmware versions, and test result.

## Phase 9 - Documentation and future host preparation

- [x] Replace the stale copied-project content in `AGENTS.md`, `README.md`,
      `HANDOFF.md`, and `SELF_CHECK.md`.
- [x] Remove the obsolete Host Link v1 protocol document and broken references.
- [x] Publish Host Link Protocol v2 and each target-profile protocol separately.
  - [x] F13 Transfer protocol implementation document.
  - [x] F13 Boot target-profile protocol document aligned with the current PC
        implementation.
- [x] Document Smart Configurator regeneration and clean-build procedure.
- [x] Document F24 board pins, LED patterns, filter defaults, CAN timing, and
      bring-up checks.
- [x] Document the F24 SRecord CRC reference, the active F13 PowerShell
      post-build CRC insertion, CS+ build-event location, CRC-only security
      boundary, and independent artifact checker.
- [x] Document how to add a target profile without modifying host firmware.
- [x] Document how an RA2L1 or RA6 host implements the same generic Host Link
      and CAN backend interfaces in `docs/HOST_BACKEND_GUIDE.md`.
- [ ] Add RA2L1/RA6 work only after the F24-to-F13 end-to-end path passes.

### Final user workflow diagrams and PDF guide

- [x] Create a user-facing Traditional Chinese Markdown guide containing
      Mermaid source diagrams based on the implemented source. Mark the guide
      as source-verified/hardware-pending until end-to-end tests are complete.
- [x] Add Mermaid sequence/flow diagrams for the Tool / MCU Host CAN transfer
      path, including:
  - [x] COM open and `GET_HOST_INFO`
  - [x] CAN/CAN FD mode, bitrate, and filter configuration
  - [x] PC Host Link request/response/event traffic
  - [x] Bridge-board CAN TX/RX queue behavior (F24 first implementation)
  - [x] Timeout, retry, disconnect, bus-off, and error reporting
  - [x] Detailed `Discover Target` sequence from Host probe through F24 CAN
        configuration/filter/start, F13 INFO pages, and conditional HELLO
- [x] Add a Mermaid diagram for the MCU Device CAN Transfer firmware flow,
      including:
  - [x] Device discovery
  - [x] PING/ECHO/PATTERN/GET_STATS commands
  - [x] Sequence and payload validation
  - [x] Normal response and every major rejection/error branch
- [x] Add Mermaid diagrams for the MCU Device CAN Upgrade firmware flow,
      including both application and bootloader behavior:
  - [x] Non-destructive app discovery
  - [x] Explicit ENTER and reset to bootloader
  - [x] HELLO and image/target compatibility checks
  - [x] START, DATA ACK/retry, flash-boundary delay, END, manifest/CRC check
  - [x] Success reset to app
  - [x] ABORT, inactivity timeout, flash failure, invalid image, CRC failure,
        interrupted power, and recovery-to-boot behavior
- [x] Ensure the guide explains what the user selects, what successful output
      looks like, how LED states correspond to each stage, and what action to
      take for each common error.
- [ ] Include final tool UI screenshots and example normal/error logs after the
      UI and log format have stabilized.
- [x] Export the verified Markdown guide to PDF with embedded diagrams and fonts.
- [x] Visually inspect every PDF page for clipped text, unreadable diagrams,
      missing Traditional Chinese glyphs, broken page breaks, and stale UI.
- [ ] Verify every diagram against the released PC tool and exact released
      firmware versions; record those versions, document date, and test setup
      in the guide.
- [x] Keep Mermaid source under version control so behavior changes require a
      matching documentation update.
- [x] Keep the visually verified generated PDF in the active project and update
      it together with the Mermaid source before release.

## Development record

For every completed phase, record:

- Date:
- Changed files:
- What changed:
- Why it changed:
- Build/test commands:
- Actual result:
- Remaining risks:

### 2026-10-12 - PC F13 Boot profile and update workflow

- Changed files: `src/core/f13_boot_profile.*`, `src/worker_session.*`,
  `src/main_frame.*`, Visual Studio project/test files, and active project
  documentation.
- What changed: added strict image validation, protocol-v2 target client,
  HELLO-only discovery, explicit ENTER, worker-owned firmware update,
  ACK-gated DATA progress, erase-boundary timeouts, and bounded ABORT/CAN_STOP
  cleanup.
- Why: connect the existing RL78/F13 boot/app protocol to the renamed native
  PC tool without moving target flash semantics into the generic F24 host.
- Build/test commands: `scripts\clean_mfc.bat`, Release/x64
  `scripts\build_mfc.ps1`, and
  `tmp\internal\build\tests\x64\Release\HostLinkProtocolTests.exe`.
- Actual result: clean x64 Release `0.1.0.0032` PASS, 0 warnings/0 errors;
  unit tests PASS; GUI launch/title smoke test PASS; the current generated
  122,880-byte F13 app image independently matches manifest target `0xF13B`,
  version `0x00010000`, and stored/calculated CRC32 `0xDA8DB250`.
- Remaining risks at that development slice: physical PC/F24/F13 communication,
  full update timing, cancel/disconnect and target-failure injection, power-loss
  recovery, the then-undecided security boundary, and linker/vector automation.
  Later records below resolve the non-hardware items.

### 2026-10-13 - Identity, UI filters, robustness, diagnostics, and automation

- Changed files: F13 boot `can_boot_config.h`, `can_boot_transport.*`,
  `r_main.c`; F13 app `custom_func.*`; PC profile/update/client/worker/UI/test
  modules; `scripts/verify_f13_artifacts.ps1`; active project documentation,
  including the generic future-host backend guide.
- What changed: added paged app/boot INFO identity, bounded target CAN waits and
  invalid-app recovery, CAN mode/data/filter UI, detailed protocol trace,
  exportable summaries with all fourteen F24 `GET_STATS` counters, a reusable
  update runner with mock fault injection, and automated linker/image/vector
  boundary checks.
- Why: make discovery non-destructive, prevent incompatible configuration or
  invalid images from progressing, improve field diagnosis, and turn static
  flash/vector assumptions into repeatable verification gates.
- Build/test commands: F24/F13 CS+ `/br`; `scripts\clean_mfc.bat` then
  `scripts\build_mfc.ps1`; `HostLinkProtocolTests.exe`;
  `scripts\verify_f13_artifacts.ps1`; GUI launch/title and ISR static checks.
- Actual result: F24 regenerated build PASS (Error 0 / Warning 2, program
  `0x75E9`); F13 boot PASS (Error 0 / Warning 5, program `0x1B95`); F13 app
  PASS (Error 0 / Warning 3, program `0x3FD4`); clean PC x64 Release
  `0.1.0.0038` PASS with 0 warnings / 0 errors; unit/mock tests, artifact
  checker, ISR check, and GUI smoke PASS. Artifact checker reports boot/FSL end
  `0x1D6F`, version `0x00010000`, CRC32 `0x0A38E6B9`.
- Remaining risks: CS+ historical device-change notice; all physical UART/CAN,
  timing, filter, LED, complete update, power interruption, runtime shared-ISR,
  and FSL failure/recovery tests; final verified Mermaid diagrams and Traditional
  Chinese PDF.

### 2026-10-13 - Host reset, events, queueing, identity, and UART cleanup

- Changed files: F24 Host Link/CAN/UART/board modules, F24 generated CAN error
  configuration, F24 automatic-build identity files, PC Host Link client,
  worker/UI/resources/tests, and active project documentation.
- What changed: implemented F24 `1.0.0 + build` identity, one active plus two
  pending generic CAN transactions, asynchronous CAN RX/state/error events,
  arbitration-loss diagnostics, bus-off/recovery reporting, ACK-before-reset
  `RESET_HOST`, and bounded PC reconnect/probe. Removed every F24 `printf`
  call and `putchar` retarget so P15/P16 carry only binary Host Link frames.
- Why: finish the non-hardware Host Link robustness work and prevent text or a
  future stdio retarget from corrupting USB-UART-VCOM protocol traffic.
- Build/test commands: F24 CS+ `/br`; `scripts\build_mfc.ps1` Release/x64;
  `tmp\internal\build\tests\x64\Release\HostLinkProtocolTests.exe`;
  `scripts\verify_f13_artifacts.ps1`; source and F24 map formatter scans.
- Actual result: F24 firmware `1.0.0.5` PASS, Error 0 / Warning 2, program
  `0x4CF2`, ROM data `0x0275`, RAM data `0x0919`; no `printf`, `_printf`, or
  `_putchar` in source/map. PC `0.1.0.0041` PASS with 0 warnings / 0 errors;
  all Host Link/CAN/F13 unit and mock tests PASS. F13 artifact verification
  PASS with 122,880-byte image and CRC32 `0x0A38E6B9`.
- Remaining risks: physical UART/CAN, reset/reconnect, queue stress, filters,
  CAN timing, buttons/LEDs, complete update, power interruption, bus fault,
  FSL failure, and shared-ISR runtime tests; final evidence-driven diagrams and
  Traditional Chinese PDF.

### 2026-10-13 - WDT policy and source-verified Mermaid flow guide

- Changed files: product/checklist/handoff/self-check documents, F24/F13
  firmware READMEs, and `docs/F13_BOOT_UPDATE_FLOW_zh-TW.md`.
- What changed: made the no-WDT product decision explicit for F24 and F13;
  documented F13 boot selection, app INFO/ENTER, shared ISR dispatch, boot
  protocol state, full update, FSL/CRC/power-loss recovery, plus secondary
  PC/F24 and non-destructive transfer flows in Mermaid.
- Why: remove future-WDT ambiguity and let new users study the implemented
  firmware behavior before the scheduled physical-board phase.
- Actual result: Markdown structure, Mermaid fence pairs, UTF-8 text,
  internal links, and terminology were statically checked. Host Link/F13
  unit/mock tests PASS; F13 artifact checker PASS with boot/FSL end `0x1D6F`
  and CRC32 `0x0A38E6B9`. Mermaid CLI rendering was NOT RUN because `mmdc` is
  not installed. No firmware or PC source changed in this documentation-only
  slice, so firmware and PC builds were not rerun.
- Remaining risks: every physical-board gate, final diagram comparison with
  captured traces, UI/log screenshots, Mermaid render review, and the visually
  verified Traditional Chinese PDF.

### 2026-10-13 - Dedicated update action and F13 boot-entry cleanup

- Changed files: PC MFC main-frame/resource files; F13 boot `r_main.c` and
  `user_def.h`; F13 boot/protocol/vector guides; architecture, workflow,
  checklist, handoff, self-check, and README documents.
- What changed: separated `Transfer Test` from a fixed
  `Start Firmware Update` control, added a live missing-prerequisite list and
  local image validation gate, removed the unrequested P13.7 boot-entry path,
  and added a detailed Tool/F24/F13 `Discover Target` Mermaid sequence.
- Why: make the destructive action unmistakable, explain exactly why it is
  disabled, and align F13 boot entry with the specified ENTER/invalid-image
  product behavior instead of inherited board-switch logic.
- Actual result: PC clean x64 Release `0.1.0.0042` PASS, 0 warnings / 0 errors.
  F13 boot clean build PASS, Error 0 / Warning 5, PROGRAM `0x1B89`, ROMDATA
  `0x0390`, RAMDATA `0x092E`. Unit/mock, artifact, UI smoke, and document checks
  are recorded after their final rerun below.
- Remaining risks: physical button-independent boot/update behavior, every
  existing hardware gate, final UI screenshots, rendered Mermaid review, and
  the Traditional Chinese PDF.

### 2026-10-14 - Profile-specific image validation and UI module split

- Changed files: `src/core/firmware_update_profile.*`, MFC main-frame modules,
  Visual Studio projects, Host Link tests, README, architecture, target-profile
  guide, flow guide, checklist, handoff, and self-check.
- What changed: completed the interrupted `main_frame` split, added a generic
  firmware-update validator catalog, moved the 122,880-byte/manifest/CRC/flash
  rules behind the F13 Boot profile, cached validation outside `UpdateUi()`,
  retained final pre-START validation, and added catalog dispatch tests.
- Why: a 120-KiB image is specific to the present F13 memory map. Future RL78,
  RA, or other CAN targets must be able to provide a different size, identity,
  address range, checksum policy, and upgrade runner without changing generic
  Host Link/CAN transport code.
- Actual result: PC x64 Release `0.1.0.0045` and Debug builds PASS with 0
  warnings / 0 errors; Release and Debug unit/mock tests PASS; both startup
  smokes find separate `Transfer Test` and disabled fixed-label
  `Start Firmware Update` controls plus the initial `Missing:` list. 100% DPI
  layout check PASS; 125% DPI is NOT RUN on the available 96-DPI desktop. F13
  artifact checker PASS with boot/FSL end `0x1D63`, image size 122,880 bytes,
  and CRC32 `0x0A38E6B9`. Workspace clean-script audit PASS for 24 scripts.
  Markdown UTF-8/link checks PASS with 11 Mermaid diagrams; Mermaid CLI render
  is NOT RUN because `mmdc` is not installed.
- Remaining risks: all physical PC/F24/F13 gates, 125% DPI/manual screenshot
  review, Mermaid visual render comparison, and the final Traditional Chinese
  PDF remain open.

### 2026-10-14 - F13 variable-length image and update flow

- Changed files: F13 boot layout/config/transport/startup validation; F13 app
  manifest and post-build generator; PC F13 profile/update runner/tests and
  artifact checker; active F13, Tool/F24, Mermaid, checklist, handoff, and
  self-check documentation.
- What changed: image format 2 now treats `0x1E000` as capacity. Post-build
  derives actual size from the linker map, patches manifest size, and appends
  CRC32 at the variable BIN end. Tool and boot use actual length for START,
  DATA count, progress, END sequence, and CRC. Tests cover 32/60/80/120 KiB.
- Why: F13 application code size changes over product life; valid aligned
  images must not be padded to 120 KiB merely to satisfy the update tool.
- Build/test commands: F13 boot/app CS+ `/bb`; app post-build hook;
  `scripts\verify_f13_artifacts.ps1`; one-click PC Release build; direct Debug
  and final Release rebuild; Release/Debug `HostLinkProtocolTests.exe`;
  Release startup smoke; UTF-8/link/Mermaid checks; workspace clean-script
  audit.
- Actual result: F13 boot Error 0 / Warning 5, PROGRAM `0x1BE1`, boot/FSL end
  `0x1DBB`, margin `0x244`; app Error 0 / Warning 3, PROGRAM `0x3FD6`, generated
  image `0x42E8`, CRC address `0x62E4`, CRC32 `0xFB79333A`. Artifact checker
  PASS. PC `0.1.0.0047` Release/Debug 0 warnings / 0 errors and all unit/mock
  tests PASS. Release startup smoke, 12-diagram static checks, and 24-script
  workspace audit PASS. Mermaid CLI rendering is NOT RUN (`mmdc` unavailable).
- Remaining risks: physical larger-to-smaller image replacement, complete
  update/power-cycle/power-loss, shared ISR runtime, FSL fault injection,
  baud/update-time/queue stress, real logs/CAN traces, screenshots, visual
  Mermaid review, and final Traditional Chinese PDF.

### 2026-10-14 - Metadata-driven CAN Upgrade v3 and UI metadata

- Changed files: PC firmware metadata/parser/client/worker/UI/settings/tests;
  F13 boot/app protocol/config/manifest/post-build; artifact checker; active
  README, architecture, workflow, checklist, handoff and self-check documents.
- What changed: replaced the MCU-name/fixed-layout update selection with the
  `CAN Upgrade` v3 metadata contract. A 96-byte app manifest and INFO pages
  `0x00..0x17` carry MCU, version/build, actual/maximum/minimum sizes, app
  start, flash/boot/erase layout, CAN modes/rates/IDs and integrity type. DATA
  now uses a 24-bit byte offset. The UI displays offline BIN size plus both CRCs
  and manifest data, then live device metadata after discovery and requires a
  complete match. `App Start Override` is persisted and mismatch-safe.
- The app post-build removes known obsolete fixed-range HEX/BIN filenames so
  old format-2 artifacts are not accidentally selected in the current tool.
- UI result: caption widths are font-measured, rows are spaced more loosely,
  `Profile` is renamed `Workflow`, filter default is `Metadata/default`, a
  valid BIN auto-selects CAN Upgrade, and image/device metadata uses readable
  multi-line rows. 100% DPI captures at 1550x830 and 1188x675 show no clipping
  or overlap; 125% DPI remains NOT RUN because no 120-DPI session is available.
- Verification: PC `0.1.0.0048` Release/Debug builds PASS with 0 warnings and
  0 errors; Release/Debug unit/mock tests and startup smokes PASS. F13 artifact
  checker PASS: boot/FSL end `0x1977`, margin `0x688`, image `0x444C`, CRC
  address `0x6448`, CRC32 `0xC05CB0B9`. UTF-8, local links, 13 Mermaid blocks,
  fence structure and the 24-script clean audit PASS. `mmdc`/PDF rendering and
  all physical F24/F13 tests remain NOT RUN.
- Remaining risks: 125% DPI, real Host Link/CAN/filter/LED/button/bus-off and
  queue timing, complete update/power-cycle/power-loss, shared ISR runtime,
  FSL fault injection, release logs/traces/screenshots, visually rendered
  Mermaid, and the final Traditional Chinese PDF.

### 2026-10-14 - Separate metadata fields and PDF delivery

- Changed files: PC metadata UI/layout/project files; F13 boot/app metadata and
  variable-image comments; PDF generation scripts; Traditional Chinese flow
  guide; README, checklist, handoff, and self-check documents.
- What changed: replaced long combined image-status rows with 16 separate
  label/value fields whose unloaded value is `N/A`; documented exact `Auto`
  app-start semantics and the map-derived variable-length CRC post-build; split
  the Tool workflow into readable discovery and programming/recovery diagrams;
  added repeatable Mermaid-to-PDF generation.
- Why: let users verify BIN identity, layout, size, and checksum offline without
  assuming a fixed 120-KiB app, and make the F13-first operating flow directly
  readable as Markdown or PDF.
- Build/test commands: one-click Release build; direct Debug and final Release
  builds; Release/Debug `HostLinkProtocolTests.exe`;
  `scripts\verify_f13_artifacts.ps1`; 120-DPI loaded/unloaded UI capture;
  `scripts\generate_f13_guide_pdf.ps1`; Poppler render/font inspection; 24-script
  workspace clean audit.
- Actual result: PC `0.1.0.0050` Release/Debug PASS with 0 warnings / 0 errors;
  Release/Debug unit/mock tests PASS. Artifact checker PASS with boot/FSL end
  `0x1977`, image `0x444C`, CRC address `0x6448`, CRC32 `0xC05CB0B9`. Actual
  125% UI captures show all 16 values and unloaded `N/A` without overlap. All
  16 Mermaid diagrams render; the 21-page Traditional Chinese PDF has embedded
  fonts and passes page-by-page visual inspection. Clean audit PASS (24 files).
- Remaining risks: final diagrams/log examples still require physical F24/F13
  behavior capture; all UART/CAN/filter/LED/button/bus-off, update/power-cycle/
  power-loss, shared ISR runtime, FSL injection, timing, and queue-stress gates
  remain hardware-pending.

### 2026-10-14 - F13 E2 Lite debug configuration correction

- Changed files: F13 boot/app CS+ projects and READMEs; English/Traditional
  Chinese interrupt-vector guides; Traditional Chinese flow guide/PDF generator;
  checklist, handoff, and self-check documents.
- What changed: boot Run-to-main now resolves to `_boot_main`; app remains
  `_main`. Both linkers enable `-OCDTR` and start normal RAM at `0xFE500`, outside
  `0xFE300-0xFE4FF` trace/self RAM. Documentation separates standalone app debug
  from integrated reset-through-boot debug.
- Why: the previous boot symbol did not exist, while boot/app RAM overlapped the
  on-chip debug trace/self RAM used by E2 Lite.
- Build/test commands: F13 boot/app CS+ `/br`; app post-build hook;
  `scripts\verify_f13_artifacts.ps1`; `scripts\generate_f13_guide_pdf.ps1`;
  PyMuPDF full-page render/font/text inspection.
- Actual result: boot PASS, Error 0 / Warning 5, PROGRAM `0x1758`, RAM data
  `0x092E`; app PASS, Error 0 / Warning 3, PROGRAM `0x418B`, RAM data `0x00A2`.
  Artifact checker PASS with image `0x444C`, CRC address `0x6448`, CRC32
  `0x6F3E6CEF`. The current PDF has 21 nonblank pages and 16 diagrams; all rendered
  pages pass visual inspection and the embedded fonts/title metadata are valid.
- Remaining risk: physical E2 Lite must still confirm halt/Continue at boot
  `boot_main` and standalone app `main`, followed by integrated boot-to-app and
  shared-ISR runtime verification.

### 2026-10-14 - Custom Transfer dual-ended comparison trace

- Changed files: F13 Custom Transfer config/protocol/README; PC CAN detailed
  trace decoder; transfer/diagnostic/Traditional Chinese flow documentation;
  checklist, handoff, and self-check documents.
- What changed: the F13 reference target now emits deterministic main-loop-only
  `TRACE CT RX/TX` records with ID, DLC, exact bytes, sequence, command, and
  CRC-8 result. PC detailed trace labels the same frames `CUSTOM_TRANSFER` and
  reports its independent CRC result. The trace is compile-time configurable
  and documented as unsuitable for throughput/timing measurements.
- Why: the PC log and a separately saved F13 UART terminal log can now be
  compared frame by frame to prove that Custom Transfer content crossed both
  bridge directions unchanged.
- Build/test commands: F13 Transfer CS+ `/br`; PC x64 Release/Debug builds and
  unit tests; final Release rebuild; clean-script audit; F13 artifact checker;
  PDF regeneration and full-page render inspection.
- Actual result: F13 Transfer PASS, Error 0 / Warning 2, PROGRAM `0x49D7`, ROM
  data `0x04BF`, RAM data `0x00CC`. PC Release and Debug PASS with 0 warnings /
  0 errors; both test binaries PASS, and the delivery executable was rebuilt as
  Release build `0.1.0.0052`; startup smoke reports the matching window title.
  Clean-script audit PASS (24 scripts); F13 artifact checker PASS
  (image `0x4668`, CRC32 `0xC98C2643`). PDF PASS with 16 diagrams, 21 nonblank
  pages, embedded fonts, valid metadata, and visual review of every page.
- Remaining risk: the source/build checks do not replace the physical capture;
  save the PC detailed `.log`, F13 terminal `.txt`, and external CAN trace.

### 2026-10-14 - Custom Transfer timeout diagnostics

- Changed files: F13 Custom Transfer protocol/startup/README; PC Host Link event
  decoder; diagnostics, protocol, Traditional Chinese guide, checklist,
  handoff, and self-check documents.
- What changed: F13 startup now reports the CAN driver result, channel status,
  compiled 500-kbit/s IDs/filter, and transceiver standby state. Its error ISR
  coalesces raw channel flags for decoded, 250-ms-rate-limited main-loop UART
  output. PC event logs retain raw F24 error values and add readable
  channel/global source names, including unknown-bit preservation.
- Why: the captured timeout showed F24 TX advancing while RX remained zero and
  `BUS_ERROR|ERROR_WARNING`; the old F13 banner could not distinguish failed
  controller initialization from physical/bit-timing faults.
- Build/test commands: F13 Transfer CS+ `/br`; PC one-click x64 Release build;
  Release `HostLinkProtocolTests.exe`; hidden startup smoke; Traditional Chinese
  PDF regeneration and render inspection.
- Actual result: F13 Transfer PASS, Error 0 / Warning 2, PROGRAM `0x4C4A`, ROM
  data `0x0663`, RAM data `0x00D2`. PC Release PASS with 0 warnings / 0 errors;
  tests and startup smoke PASS; delivery build is `0.1.0.0054`. The regenerated
  PDF has 16 diagrams and 21 nonblank pages with embedded fonts; all pages and
  the changed diagnostic/table pages pass rendered visual inspection.
- Remaining risk: the electrical root cause remains hardware-pending. Reflash
  F13 and preserve its new startup/error/RX/TX log with the PC log and external
  CAN trace.

### 2026-10-14 - F24 zero-ID/zero-DLC TX correction

- Changed files: F24 `host_link_protocol.c`; checklist, handoff, and self-check.
- What changed: CAN TX records now produce the Renesas four-word packed header
  directly and validate the compiler bit-field view. Large transaction/frame
  copies use explicit `memcpy` across pending, queued, and active states.
- Why: two physical tool logs showed TX scheduling and timeout; PCAN captured
  the initial request and retry as Classical ID `0x000`, DLC `0`, rather than
  the required standard ID `0x710`, DLC `8`.
- Build/test command: F24 CS+ `/br`.
- Actual result: PASS, firmware `1.0.0 build 20`, Error 0 / Warning 2, PROGRAM
  `0x4D12`, ROMDATA `0x0275`, RAMDATA `0x0919`. Warnings are the existing
  expired/missing CC-RL paid-license warnings.
- Remaining risk: reflash and physical PCAN/F13 confirmation are required; do
  not mark the Custom Transfer physical gate complete until `0x710`/DLC 8,
  F13 `TRACE CT RX`, and the `0x711` response are captured.

### 2026-10-14 - Periodic F24 CAN analyzer test

- Changed files: PC UI/resource/layout, worker session, README, architecture,
  diagnostics, checklist, handoff, and self-check.
- What changed: a fixed Start/Stop button runs a cancellable background test
  that sends standard ID `0x123`, DLC 8, data
  `11 22 33 44 55 66 77 XX` every 1000 ms at Classical CAN 500K. `XX` resets
  to `00` on Start and increments for each frame. A `?` tooltip beside the
  button explains that a CAN analyzer is required. The test uses the existing
  generic Host Link commands and does not wait for an F13 response.
- Why: PCAN continued to report `0x000`, DLC 0 although the Tool UART trace
  encoded `0x710`, DLC 8. The periodic test isolates F24 controller/pin/
  transceiver output from target discovery and retry behavior.
- Software result: Release and Debug x64 builds PASS with 0 warnings and
  0 errors; Release/Debug protocol tests PASS; Release `0.1.0.0056` startup
  smoke opens a responsive main window. The available UI automation surface
  did not expose native windows, so screenshot-based visual QA is NOT RUN.
  Physical PCAN result remains pending.

### 2026-10-14 - Debug/Release output isolation

- Changed files: Visual C++ project, build script, README, checklist, handoff,
  and self-check.
- What changed: Release x64 compiles to
  `tmp\internal\build\release\x64\FirmwareUpgradeTool.exe`; Debug outputs use
  `tmp\internal\build\debug\<platform>\`, and Release Win32 uses
  `tmp\internal\build\release\Win32\`. After a successful Release x64 build, the build
  script copies the EXE to the stable `build\FirmwareUpgradeTool.exe` delivery path
  and only bumps the product version for Release. Because no MSBuild target
  owns the delivery path, Debug clean/rebuild cannot remove it.
- Why: all configurations previously shared `build\`, allowing the roughly
  13-MB Debug binary and large Debug PDB to overwrite the roughly 2–3-MB
  Release delivery files. A later incremental Release build was not guaranteed
  to relink the shared target.
- Actual result: Release x64 `0.1.0.0058` PASS, 0 warnings / 0 errors and
  2,928,640-byte delivery EXE. Debug x64 Rebuild PASS, 0 warnings / 0 errors
  and 13,401,088-byte Debug EXE. The delivery EXE remained present with an
  unchanged SHA-256 after Debug Rebuild. Release/Debug protocol tests, Release
  startup smoke, and the 24-script clean audit PASS.

### 2026-10-14 - Generic bridge-board self test

- Changed files: Tool UI/workflow/worker text, diagnostics and architecture
  documentation, checklist/self-check/handoff, and F24 Host Link frame decoder.
- What changed: the user-facing `F24 TX Test` name is now `Bridge Board Self
  Test` so the same Host Link v2 operation can be used by future F14 and RA6
  host bridges. F24 frame construction now follows the supplied Renesas CAN and
  CAN FD examples by assigning the public `Head.Bits` fields directly.
- Why: the build-22 hardware trace proves F24 transmits at the correct period
  without bus errors, but the actual frame remains ID `0x000`, DLC 0 rather
  than ID `0x123`, DLC 8. Physical retest of the rebuilt F24 firmware remains
  required.
- Actual result: F24 build 23 PASS, Error 0 / Warning 2, PROGRAM `0x4D63`, ROM
  data `0x0275`, RAM data `0x0919`. PC Tool `0.1.0.0059` Release/Debug PASS
  with 0 warnings/errors; Release/Debug protocol tests, Release startup smoke,
  and the 24-script clean audit PASS. Reflash/build-23 PCAN verification and
  final visual inspection of the longer button caption remain NOT RUN.

### 2026-10-14 - F24 TX RAM diagnostic extension

- Changed files: F24 CAN host module/Host Link response, Tool CAN client and
  worker logging, Host Link v2 protocol document, protocol tests, checklist,
  self-check, and handoff.
- What changed: F24 captures both driver input and TX message-buffer RAM
  read-back. `CAN_TX` optionally expands from 8 to 40 response bytes while
  preserving the original first eight bytes; Tool supports both forms and logs
  a `Bridge TX diagnostic` line.
- Why: build-26 hardware evidence still shows error-free frames with ID
  `0x000`, DLC 0 despite correct Tool requests and successful completion. The
  added evidence separates transaction corruption, RAM/window mapping, and
  controller-buffer selection.
- Actual result: F24 build 27 PASS, Error 0 / Warning 2, PROGRAM `0x4E4D`, ROM
  data `0x0275`, RAM data `0x0939`. Tool `0.1.0.0061` Release/Debug PASS with
  0 warnings/errors; extended-response tests, both protocol-test binaries,
  Release startup smoke, and 24-script clean audit PASS. Reflash/build-27
  physical read-back remains NOT RUN.

### 2026-10-14 - F24 CAN clock correction

- [x] Verify build-30 driver input versus hardware TX-RAM read-back evidence.
- [x] Verify the manual's fCLK=fMP/2 and external CAN-clock requirements, and
  the schematic's fitted Y3 4-MHz crystal.
- [x] Add bounded runtime X1/PLL selection with MDIV=1 outside generated code;
  retain HOCO startup for CS+ main breakpoint and HOCO fallback on failure.
- [x] Start UART/timer only after the clock decision; keep their CPU clock at
  40 MHz and refuse CAN when runtime clock validation fails.
- [x] Report the clock-initialization stage through binary Host Link and Tool.
- [x] Run real-clock-module mocks for normal, repeated, oscillator/PLL failures,
  fallback, clock loss and unexpected startup; protocol legacy/error tests PASS.
- [ ] Reflash corrected firmware and confirm PCAN 0x123, DLC 8, payload counter
  and matching TX-RAM read-back; capture external-clock fallback/debug evidence.

Changed files: f24_can_clock.c/.h, can_fd_module.c, custom_func.c,
host_link_protocol.c, CS+ project/export, Tool can_host_client.cpp, protocol and
clock tests/test project, README/F24 bring-up/diagnostics/protocol docs,
SELF_CHECK.md and HANDOFF.md. No generated driver/configuration was hand-edited.

Actual result: F24 build 32 clean rebuild PASS, Error 0 / Warning 2 (existing
license warnings), PROGRAM `0x512F`, ROMDATA `0x0276`, RAMDATA `0x093A`.
Tool 0.1.0.0062 Release/Debug builds and both protocol/clock test binaries PASS,
0 warnings/errors. Both startup smokes PASS; 24-script workspace audit PASS.

### 2026-10-14 - 20-MHz regenerated clock/pin configuration follow-up

- [x] Review regenerated X1 20 MHz, HOCO 40 MHz and preserved UART/timer/
  button callbacks; identify unbounded pre-main X1 wait, P73/P72 route and
  nominal/data SJW greater than TSEG2.
- [x] Add non-generated f24_board_config.h as the CS+ compiler preinclude:
  defer X1 start, force P11/P10, correct SJW to 16/6; keep generated files intact.
- [x] Use valid 20 MHz /4 x16 -> PLL 80 MHz -> MDIV=1 clock chain; never
  rewrite CMC, set PLL dividers atomically, stop PLL before HOCO fallback.
- [x] Read-only CMC/configuration/rate/routing/PLL/timeouts tests PASS in
  Release/Debug; actual CC-RL startup/driver preprocessing and assembly PASS.
- [x] Final F24 clean rebuild: build 37, Error 0 / Warning 2 (license), PROGRAM
  0x517F, ROMDATA 0x0276, RAMDATA 0x093A. One-click Debug x64 build and 24-script
  clean audit PASS. Generated-file SHA-256 comparison PASS (66 unchanged).
- [x] Record supplied build-34 185719.log evidence: UART115200 host identity,
  RESET_HOST ACK and reconnect PASS; CAN setup failed before any TX.
- [ ] Flash final 20-MHz firmware and capture debugger-main, clock/fallback,
  PCAN ID123/DLC8/counter data and TX-RAM read-back. Hardware remains NOT RUN.

Changed files and detailed verification/evidence locations: HANDOFF.md,
2026-10-14 20-MHz follow-up. Tool Release 0062 is unchanged and compatible.
The following UART-preset slice implements the previously deferred 250K/1M work.

### 2026-10-14 - UART 250K/500K/1M bitrate presets

- [x] Define wire preset mapping 1=250K, 2=500K, 4=1M; preset 3 remains
  800K and is rejected. F24 reports nominal capability mask 0x16.
- [x] Apply validated timing in global/channel reset mode on SET_CAN_CONFIG;
  reapply after generated R_CAN_SetConfig during CAN_START. No generated edits.
- [x] Show only host-advertised rates, default 500K with existing INI restore;
  selection changes use worker-owned UART command and leave CAN stopped.
- [x] Self Test and generic target setup use selected rate; retain F13 image/
  device metadata gates and no implicit target-rate change.
- [x] Mock tests verify six Classical/FD UART payloads, old-host capabilities,
  invalid zero-traffic configurations, all invalid firmware presets and
  timing/register/clock/controller-mode guards. Release/Debug tests PASS.
- [x] F24 build 41 /br PASS, Error 0 / Warning 2 license, PROGRAM 0x52E9,
  ROMDATA 0x0276, RAMDATA 0x093A. Tool 0063 Release/Debug PASS, 0 warnings/errors;
  clean audit PASS (24 scripts).
- [ ] 0063 native GUI startup/visual smoke: NOT RUN, Computer Use pipe unavailable.
- [ ] Physical UART ACK, Classical 250K/500K/1M and FD nominal/data trace,
  rate changes after stop, and unchanged UART115200/reconnect.

Changed files and verification details are recorded in the corresponding
HANDOFF.md slice. Default F13 firmware remains Classical 500K.

### 2026-10-14 - COM device-name UI and physical Custom Transfer evidence

- [x] Add read-only `serial_port_catalog` with Windows FriendlyName/DeviceDesc
  metadata, DOS-name fallback, numeric sort and deduplication. Keep UI item
  data, INI and transport based on canonical COM names; never auto-probe ports.
- [x] Add measured wider COM/dropdown, responsive Open/Close/Reset row and
  selected-adapter description in the queued probe log.
- [x] Release/Debug x64 builds PASS, 0 warnings/errors, Tool0065. Both full
  unit/mock binaries PASS, including COM metadata, invalid/absent/high-number
  ports, Unicode and fallback. Clean-script audit PASS (24 scripts).
- [ ] New native startup/visual/100%/125% DPI and live COM/INI checks: NOT RUN,
  native Computer Use pipe unavailable; local enumeration has no COM devices.
- [x] Supplied Tool0063/F24build40 `201510.log` and F13 UART frames confirm
  DEVICE_INFO/PING/ECHO/PATTERN_TEST/GET_STATS through the complete 500K path:
  22 requests/replies, matching payloads/CRC, zero final F24 error counters.
- [x] Record F13 TJA1050T EVDD supply fault: 3.3V failed; user retested at 5V
  with valid PCAN/F13 responses and no bus errors. S/P12 LOW is correct.
- [ ] Physical CLEAR_STATS, error/stress/queue/filter rejection, 250K/1M/FD,
  buttons/LEDs and Boot/App update/power-loss/shared-ISR tests remain open.

Changed-file list, actual commands/results and remaining checks: HANDOFF.md,
COM friendly-name slice. Historical hardware-pending records above are
superseded only for the explicitly evidenced normal 500K transport checks.

### 2026-10-14 - Align F13 boot/app to the verified communication benchmark

- [x] Inspect Transfer pcan.jpg/log.jpg and preserve the successful reference.
- [x] Share startup/clock/CAN reset support between boot/app: X1=4M, PLL/CPU32,
  CAN16, 500K 75% sample point; preserve a valid inherited PLL at app startup.
- [x] Defer X1 waits until main, reject clock failures before CAN/FSL, mask
  boot UART's app-only interrupts, retain boot/app 0x700/0x701 and shared ISR.
- [x] Add C90 real-source mocks for all clock stages/fallback, CAN RAM/mode
  timeout, invalid divider and boot-to-app hardware inheritance.
- [x] Final paired CS+ clean builds PASS: boot Error0/Warning5, app
  Error0/Warning3 (existing toolchain/vector/output warnings). Artifact checker
  PASS: boot/FSL end1C85, margin37A, image4AD8, CRC at6AD4 = 5AAE684B,
  version1.1.1. Shared-source registration/startup-hook guards also PASS.
- [x] Release test-project build and Debug one-click build PASS, 0 warnings/
  errors; both full test binaries PASS. Workspace clean audit PASS (24 scripts).
- [x] Regenerated and visually reviewed 22-page Traditional Chinese PDF with
  16 Mermaid diagrams; includes benchmark clocks, 5-V EVDD, initial paired
  flashing and read-only analyzer INFO examples. Boot decision uses a compact
  vertical diagram rather than an unreadably wide/long layout.
- [ ] Flash the new paired image on the 5-V EVB and verify INFO/ENTER/update;
  do not combine a new boot with the previous HOCO-only valid app.



