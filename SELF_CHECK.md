# Host Link v2 Self-Check

## Public resources and portable checkout (2026-10-15)

- [x] Tool EXE, F24 Boot MOT/App BIN, F13 Boot HEX/combined HEX/App BIN
  all HTTP200. Both web guides expose downloads and six workflow/document
  entries; public Markdown has no private workspace paths or release pins.
- [x] README/porting guides explain full F24 checkout and sibling F13 Boot/App,
  ZIP renaming, build order and initial-programmer vs App-only installation.
  Public F13 source/library/project comparison136matches; registered paths
  Boot29/App28/Transfer35 exist. Downloaded App BIN CRC/hash matches local;
  HEX differences are Git line endings, normalized content matches.
- [x] Normal Tool Debug/Release solution builds without F13 configuration and
  explicit test Debug/Release builds all0warnings/errors. Both actual-BIN suites
  PASS; copied neutral sibling test build/run PASS, missing dependency fails
  with actionable diagnostics. Runtime/firmware code and six artifact hashes unchanged.
- [x] Guide/public/path tests15 PASS; canonical5/web198refs/six anchors and
  six URLs/preview HTTP/safe-clean24 PASS. Four PDFs30/10/13/10 pages rendered
  with20/5/7/2 diagrams and visually reviewed; HTML anchors no longer print.
- [x] Removed unused generated diagrams, quality reports and private IDE/cache
  files by recoverable moves; .gitignore excludes regeneration. PDF Git binary
  attributes prevent text conversion; no Git stage/commit/push/deployment.
- [ ] Full Python35:34PASS/1optionalSKIP plus F13 IRQ class setupERROR, missing
  linker maps. Fresh CC-RL/map audit and hardware/browser/DPI NOT RUN.
  Generated SC/project-wizard provenance can still contain local paths;
  prepare_f24_bridge_projects relocates those, actual source paths are relative.

## Evergreen public web copy (2026-10-15)

- [x] Linked RL78 ISP Tool canonical URL; Bootloader navigation/intros use
  function descriptions without draft/publication labels, release pins or
  single-run benchmark marketing. Canonical blocks remain synchronized.
- [x] Product/firmware binaries unchanged; exact release records preserved.
  Protocol/image/IRQ requirements and unverified-feature warnings retained.
- [x] Four new public-web tests PASS, including a fake future Tool version;
  verifier no longer demands a build number in the guide. PS5 Unicode-safe.
- [x] Final public/guide/PDF/porting tests10 PASS; canonical sync4 and197
  local references/fences PASS. HTTP7 standard routes plus navigation and
  both Markdown sources return200. Safe clean audit24 PASS; no clean deletion.
- [x] PDFs30/10/11/9 pages regenerated with20/5/6/2 Mermaid diagrams;
  all rendered pages/contact sheets and changed detailed pages reviewed.
  Tool EXE SHA unchanged; no runtime rebuild, COM/flash/Git/publishing.
- [ ] Actual browser rendering/DPI visual acceptance NOT RUN; HTTP/source
  and PDF review are not a browser screenshot. Public site deployment untested.

## First engineering delivery (2026-10-15)

- [x] User182253 log read: COM6/115200 F24App1.5.0/all256 metadata bytes,
  CAN500K F13v5,19244B,301 ordered ACKs/next addresses,10963ms/1755B/s,
  END CRC CA0DF55C; no reported retry/timeout/queue overflow/CAN error.
- [x] Freeze measured Tool0082/F24Boot1.2.0-App1.5.0/F13Boot-App1.5.0.
  All six delivery artifact hashes/sizes match the first-delivery manifest.
  No product/firmware rebuild, version bump, COM/flash/reset or publishing.
- [x] First-delivery guide lists binaries, initial-vs-app-only installation,
  sibling source dependencies, privacy/library-license cautions and limits.
- [x] Rebuilt test executables only: Debug/Release0warnings/errors, both
  actual-image suites PASS. Six hashes, Intel HEX checksum/EOF/Boot range,
  combined-vs-Boot/App bytes, App CRC and reserved128FF checks PASS.
- [x] Final guide/PDF tests6 PASS; canonical sync4/web196/current0082 and
  safe clean audit24 PASS. PDFs30/10/11 pages regenerated/rendered; all
  contact sheets plus all six changed-evidence pages visually reviewed.
- [ ] Full Python discovery had24 PASS/1optional fixture SKIP/1setup ERROR:
  F13 map files were removed. Full map/symbol/IRQ artifact audit NOT RERUN;
  use preceding clean-build evidence and identical delivery hashes, not a
  fabricated current pass. Details: tmp/internal/first_delivery/VERIFICATION.md.
- [ ] New v5 post-reset App execution, repeated/high-baud performance, native
  DPI/ghosting and real disconnect/power-loss/bus-off/IRQ acceptance pending.

## CAN v5 block update0082 (2026-10-15)

- [x] User171150 old-v4 baseline77s DATA/79s overall recorded; incomplete trace
  acknowledged. New UI queue-drop warning/counter and log-first eviction added.
- [x] F24 generic22/bit12 full validation before TX,122-byte max burst and
  nonblocking physical TX/gap/final reply including fast ACK; no F13 semantics.
- [x] F13 v5/INFO1A/64-byte blocks, indexed raw8-byte data, CRC/metadata before
  Flash, last-block replay without rewrite and bounded target-profile retry.
- [x] Real PC/parser/runner + actual F24 codec/progression + actual F13 RAM/CRC
  tests:16 tail lengths, missing/corrupt/conflicting segments, lost/wrong ACK,
  Flash error/disconnect/cancel, modeled word power cuts and actual packaged BIN.
- [x] F13 pair1.5.0 clean CC-RL:0errors;8KiB/App2000/F1V1 preserved,530bytes
  spare and512guard, IRQ/HEX/RAM/layout/metadata/CRC artifact checks PASS.
- [x] F24 App1.5.0 clean build42696B/CRC864E811E;0errors/2license notices,
  existing Boot1.2.0 MOT SHA unchanged. Misc remains115200.
- [x] Debug/Release0082,0warnings/errors; both actual-image C/C++ suites PASS.
  Python29 PASS,1optional real CC-RL A/B fixture-build test explicitly skipped.
- [x] MD/web/canonical sync4/navigation/screenshot gates; four PDFs30/10/11/9
  pages rendered/visually reviewed; web196/current0082; safe clean audit24 PASS.
- [ ] Native startup/DPI/screenshots NOT RUN: Computer Use native pipe/os error2.
- [ ] v5 real-board throughput/USB-UART stress/bus-off/power-loss/IRQ NOT RUN;
  install matching firmware and save new logs before claiming physical speed.

## PC serial latency0080 (2026-10-15)

- [x] User0079 log164225: one real F13 update19224B/CRC1DE10DCD,4806 DATA
  frames/about142s, zero reported errors/retries; UART screenshot shows V2 App.
- [x] Actual SerialPort Win32 I/O model regression RED before fix/GREEN after;
  short replies return without full512bytes/20ms delay. Bound idle reads50ms,
  caller deadline/zero poll/wrap/cache/error/cancel preserved. No driver change.
- [x] Real SerialPort+parser partial/max128/coalesced/CRC-bad response recovery
  and4806 Host Link exchanges PASS; these are deterministic model tests.
- [x] Release/Debug one-click and test builds0warnings/errors; both full actual
  F24/F13 packaged-image suites PASS.0080 delivery matches compiled Release.
- [x] INI and all four F13/F24 firmware artifact hashes unchanged. No board IO,
  flashing, firmware rebuild, protocol/baud default or generic batch change.
- [x] MD/web/navigation/screenshot gate and three PDFs30/10/11pages synced and
  visually checked; startup3/route2/artifact3/guide2/porting4/sync4/web196/clean24 PASS.
- [ ] Native startup/DPI NOT RUN: Computer Use list_apps native pipe/os error2.
  Physical0080 throughput/FTDI/high-baud/full fault matrix still pending; first
  use same BIN/115200/500K/same trace as0079, then change trace/baud separately.
  Detailed reproduction: tmp/internal/serial_latency/VERIFICATION.md.

## App metadata reconnect / startup settings0079 (2026-10-15)

- [x]154213 log/source identifies missing outer READ_METADATA57 route; App
  route regression was RED before fix and GREEN after. Mandatory readback kept.
- [x] Actual App C handler/common engine with PC readback and packaged BIN:
  two sessions/four64-byte pages each, bounds, identity and no writes PASS.
- [x] App1.4.1 clean CC-RL build0errors/2existing license warnings, package/
  IRQ/RAM guards PASS:40944B/CRC9F32E2B8. Boot MTPJ/vector/MOT unchanged.
- [x] Startup guard enabled from construction; EN_CHANGE/save/empty INI path
  guarded; real error diagnostics retained.3 source lifecycle tests PASS.
- [x] Release0079 delivered; Debug/Release0warnings/errors, final full PC
  suites with actual F24/F13 BIN PASS. Existing INI hash unchanged.
- [x] MD/web/recovery text and PDFs30/10/11/9pages synced; affected PDF pages
  visually checked. Guide2/porting4/artifact3/layout4/sync4/web195/clean24 PASS.
- [ ] Actual SW1 reset -> BOOT115200 -> reload1.4.1 -> Misc update -> explicit
  RUN -> close/reopen Probe NOT RUN; no COM/reset/flash performed. Native GUI
  startup NOT RUN (prior native pipe failure not re-tested this turn).
  Details: tmp/internal/app_readback_fix/VERIFICATION.md.

## Progress footer clipping0078 (2026-10-15)

- [x] Reproduced the old145px wrapping defect with real Segoe UI/GDI:
  old30/40/50px heights versus single-line widths171/215/258px at96/120/144dpi.
- [x] main_frame.cpp/.h/layout and ui/console_geometry.cpp/.h now measure
  progress caption with100% reserved, forbid wrapping and bound all footer
  children. No per-packet whole-window layout; operation-name change only.
- [x] console_bridge_tests.cpp covers8labels/3widths/3fonts,0/9/40/99/100%,
  no overlap/client spill, unsupported narrow widths and original-defect proof.
- [x] Release and Debug one-click builds0warnings/errors; actual-image full
  PC suites after final test rebuild PASS. Release EXE0078 delivered; INI and
  F24/F13 Boot/App artifacts unchanged. No firmware or wire changes.
- [x] MD/local web/navigation and PDF current versions synchronized; diagrams
  20/5/6 rendered, PDFs30/10/11pages checked (F13 changed page1 visually;
  shared/multi all pages). PDF2/porting4/artifact3/F24-layout4 tests PASS.
- [x] Canonical sync4/web195/current0078/workspace clean audit24 PASS.
- [ ] Native Debug/Release startup, actual update/resize/TAB/DPI screenshots
  NOT RUN: Computer Use native pipe missing/os error2. GDI tests are not
  physical desktop/UI acceptance. No COM/flash/publish actions.
  Exact commands, hashes and changed files: tmp/internal/progress_footer/VERIFICATION.md.

## F24 two products / shared board sources (2026-10-15)

- [x] Only Boot/App MTPJs under active firmware. Shared f24_board has no main,
  MTPJ or flash output; runtime ownership and portable I2C engine separated.
- [x] All213 original files accounted for; only four intended include/build
  boundary changes. Retired standalone project preserved under tmp/internal.
- [x] Own-project options/hooks/JSON preserved; paths resolve; preparation
  byte-idempotent; isolated new/removed SC source reconciliation tested.
- [x] Both clean CC-RL builds0errors/2existing license warnings, packaging/RAM/
  ISR guards PASS; Boot MOT/App BIN byte-identical to baseline.
- [x]5 real App A/B mapping/identity fixtures preserve Boot vector/MTPJ/MOT;
  Debug/Release PC builds and full actual-image test suites PASS.
- [x] F13 artifact/3IRQ checks, structure4/artifact3/guide2/porting4 tests,
  canonical4/web195/version0077/Mermaid11+9/clean24 PASS. PDFs10+9pages reviewed.
- [x] Tool EXE/INI/0077 unchanged; MD/web describe Boot-only installation then
  Misc115200 App update, as well as programmer installation of matching pair.
- [ ] SC GUI regeneration at new shared path and physical Boot/App recovery,
  flashing, IRQ and UART testing NOT RUN. No native GUI, COM or publication.
  Detailed reproduction/changed files: tmp/internal/F24_TWO_PROJECTS.md.

## Disposable internal artifacts (2026-10-15)

- [x]13 historical records/plans and build artifacts relocated to tmp/internal;
  416 file hashes preserved at move time; no deletion or source/package move.
- [x] Current guides/specifications remain in docs; current EXE/INI stays build.
- [x] Future PC/test/compiler/CS+ log/evidence paths use tmp/internal/build;
  temporary directories are recreated without depending on archived records.
- [x] Debug entry/Release direct/test-project builds0warnings/0errors, both PC
  suites,5 real F24 fixture tests, layout3/porting4/PDF2 tests, close mocks,
  sync4/web195 and clean24 PASS. Delivery EXE/INI hashes and0077 unchanged.
- [ ] Actual GUI startup/clean deletion/hardware NOT RUN; folder/configuration
  only, no product UI or firmware code changed. Details in tmp/internal/CLEANUP.md.
- Historical evidence links below are optional and become unavailable after
  tmp cleanup. Current contracts never require those reports to build/run.

## Boot/App porting documentation (2026-10-15)

- [x] One firmware-developer MD entry,9-page reviewed PDF, canonical web
  technical block and ISP/navigation entry; legacy callback tutorial labeled.
- [x] F13/F24 partitions/core128/reserved128, startup/RAM/IRQ differences,
  F13 AA/55 versus F24 00/A5, CS+ hooks and actual filename limitations stated.
- [x]4 new sync/read-only/link/PDF tests, existing2 PDF tests, F13 artifact/
  3 IRQ-model tests, F24 generation CheckOnly and all4 sync checks PASS.
- [x]195web refs/current0077/7HTTP, Boot11+ISP9 Mermaid,9-page PDF visual
  review and clean24 PASS; delivered EXE/F13/F24 binary hashes unchanged.
- [ ] Browser layout/native GUI/hardware NOT RUN; no firmware rebuild or
  traffic required for documentation-only changes. No web publication.
  Exact changed files, commands and results: tmp/internal/docs/RL78_PORTING_GUIDE_VERIFICATION.md.

## Host Link baud0077 (2026-10-15)

- [x] Independent Host Link115200/460800/921600 negotiation; APP bit11/05..07.
  ACK drain, confirm2s, stalled-TX fallback, tick wrap and bounded queues.
- [x] Selected-COM recovery, normal-close restore, Reset board-ID check,
  read-only fresh-sequence collision recovery; no Flash-command retry change.
- [x] BOOT fixed115200 and original MOT hash preserved. Misc ENTER reconnects
  at descriptor rate. Existing CAN/BUR1 and F13 images remain unchanged.
- [x] DEVICE UART policy2 fixed-baud parser/UI/gate/build helper; target UART
  adapter/tab/pins/programming NOT enabled, G16 replacement still deferred.
- [x] PC Release/Debug0warnings/0errors and full actual-package suites PASS;
  F24 split boot/app/monolithic builds0errors/2license warnings; F13 audit PASS.
- [x]5 real A/B IRQ tests/unchanged boot, canonical MD/web checks194refs/
  0077/7HTTP/9+9charts, PDF30/10/11pages visual review and2 PDF tests PASS.
- [x] Clean audit24 PASS. Actual logs: tmp/internal/docs/HOST_LINK_BAUD_VERIFICATION.md.
- [ ] Physical FTDI/UART460800/921600/CRC/overflow/throughput/update/reset/
  disconnect/Misc ENTER and native startup/TAB/DPI NOT RUN (pipe error2).

## Common metadata policy0076 (2026-10-15)

- [x]256-region/core128/reserved128, UPD1 role/transport/IRQ ABI/minboot;
  CAN format6/protocol4, BUR1contract3. Initial matching boot/app required.
- [x]18 shared UI fields, original BIN identity across TAB, red wrong route;
  independent worker/device checks, wrong-CRC/role/transport rejection.
- [x] F13 RESET/stage/seal before START, exact DATA metadata/replay checks;
  real C routine tests, metadata timeout/disconnect/ACK failure, two768-cut
  flash models. These are NOT physical power-loss tests.
- [x] Final PC Release/Debug0warnings/0errors/full suites exit0; F24 clean
  boot/app/package and5 real A/B app-only/boot-hash tests plus both PC suites.
- [x] F13 clean builds/artifact/3IRQ tests: end1DD1/558spare, app19224/
  CRC1DE10DCD. Fixed vector contracts and original ROM/RAM/IRQ guards retained.
- [x] Clean audit24, exact canonical MD/web blocks,194refs/version0076/
 7HTTP routes/8+8Mermaid. Three active PDFs30/9/10pages visually reviewed;
 2 PDF tests PASS. Full evidence: tmp/internal/docs/COMMON_METADATA_VERIFICATION.md.
- [ ] Native startup/TAB/DPI: NOT RUN, native pipeos error2 after recovery.
- [ ] Real update/IRQ/debug/disconnect/power cuts/max-image CRC latency:
  MANUAL NOT RUN; no board programming or COM session this turn.
- [ ] G16 replacement I2C boot/app: explicitly DEFERRED; old programming stays
  BLOCKED. UART device adapter, signatures and anti-rollback not implemented.

## F24 independent app-local IRQ engineering flow (2026-10-15)

- [x] Per-app map/identity generation, CS+ hooks, -AppProject build/package;
  no boot writes; reference -Kind app retains local JSON and isolates boot.
- [x] MCU/split flag/registration/no-vect/global/normal ISR checks; actual64
  branch targets and RETI/RETB checked; previous flash/RAM/stack/RFD/CRC gates.
- [x] Five tests including real A/B CC-RL builds and boot vector/MTPJ/MOT hash
  isolation PASS. A/B differ in added IRQ symbols/vectors and app identities.
- [x] A/B actual BINs each PASS the PC parser/mock uploader/full suites;
  no COM, flash programming or physical IRQ execution involved.
- [x] Final reference boot/app clean0errors/2license warnings; boot MOT same
  hash after explicit regression, reference BIN38848/CRC15D3F05B same size/CRC.
- [x] PC0075 EXE untouched; one-click Debug0warnings/0errors, both complete
  actual-package suites PASS; unchanged F13 artifacts PASS; clean audit24.
- [x] Canonical F24 and existing F13 web blocks match;194refs/current0075,
  ISP7/Boot7 diagrams render PASS. Shared PDF7pages/3diagrams all reviewed,
  final changed pages re-reviewed; parser2tests PASS, other PDFs untouched.
- [ ] Physical app swaps/new IRQs/UART ENTER/E2 lite/reset/power loss NOT RUN.
- [ ] GUI startup/visual NOT RUN this turn: unchanged GUI; prior native pipe
  failure remains open. No new I2C device bootloader/standalone build claim.
- [x] No programming/reset/COM workflow, INI hand edits or commit/push/deploy.

Evidence and exact files: tmp/internal/docs/F24_APP_LOCAL_IRQ_VERIFICATION.md.

## F13 fixed complete IRQ interface0075 (2026-10-15)

- [x] Reviewed prior source/artifacts; preserved existing console/clock/RAM/
  protocol work. F24 complete forwarding existed; F13 callback gap corrected.
- [x] All61 boot normal positions; owner/default rules; no competing VECTN;
  app-local mapping and identity; all normal ISRs RETI, BRK not forwarded.
- [x] Format5/app1.3.0/boot1.3.0; valid-CRC legacy4 rejection; initial paired
  install and different-app contract documented. CAN protocol3 unchanged.
- [x] F13 clean builds0errors (boot4warnings, app3); artifacts PASS boot1DC7/
  568free, app4AF0/CRCFC1DDC75; guard512 unchanged. Three IRQ tests PASS,
  7808 actual-opcode stack cases and app-only mapping/version isolation.
- [x] Release/Debug PC0warnings/0errors0075 and full actual-package suites
  PASS; clean audit24. F24 firmware/bridge contract unchanged.
- [x] MD/target bilingual guides/canonical web blocks synchronized;194refs/
  current version, PDF2tests PASS; all40 PDFpages visually reviewed, F13
  changed pages re-reviewed, final27/5/8pages and18/2/4diagrams.
- [ ] Native startup/UI NOT RUN: native pipe os error2 after retry/reset.
- [ ] Physical matching install, new app-only IRQ without boot reflash,
  timer/UART/INFO/ENTER, E2 lite halt/continue/stop, update/reset/power loss,
  WDT/brownout MANUAL NOT RUN. No board or publication action performed.

Exact file list, reproduction and evidence: tmp/internal/docs/F13_FIXED_IRQ_VERIFICATION.md.

## Left-only TABs / full F13 boot decision0074 (2026-10-15)

- [x] TAB confined to left settings; independent persistent shared image/log
  headings; one existing BIN/16-field/log pool, saved tab keys unchanged.
- [x] New geometric separation/heading checks and existing minimum/1366/large/
  larger-text cases PASS. No reparenting or protocol/MCU behavior change.
- [x] Release/Debug one-click0warnings/0errors; both full actual-package suites
  PASS. Unchanged F13 artifact audit PASS; clean audit24 PASS.
- [x] Complete F13 entry/header/reserved/size/version/layout/CAN/CRC gates,
  ENTER/handoff, timer enable/refresh/ABORT/error/reset paths in MD and both
  web pages. Canonical marked-block comparison PASS; stale target MD corrected.
- [x] Web194refs/version0074, ISP5/Boot5 Mermaid render PASS. Three PDFs final
  5/8/25pages,2/4/17charts rendered and all38pages visually reviewed; parser/
  output2tests PASS. Final delivery hash9B9B453E...24AC4ECA matches Release.
- [ ] Native startup/1366/100%/125%/TAB/no-ghost screenshots NOT RUN: native
  pipe os error2 after retry/reset. Geometry tests are not visual acceptance.
- [ ] HTTP/browser preview NOT RUN: launch blocked by policy; no bypass.
- [ ] Physical F13 boot/update/timeout/CAN-FSL fault gates MANUAL NOT RUN.
- [x] No MCU source/options changed or firmware recompiled; no programming/
  reset/test traffic initiated, no INI hand edits, no commit/push/deploy.

See [verification record](./tmp/internal/docs/LEFT_TABS_BOOT_DECISION_VERIFICATION.md).

## Host128 / metadata256 delivery0073 (2026-10-15)

- [x] Payload128/frame142 single source; explicit96-host rejection;256-byte
  UART ring capacity guards; I2C derived125-byte chunks/fetch.
- [x] Metadata region256/core96/reserved160FF, unchanged code offset200h;
  bridge contract2 four-page STAGE/READ and CRC-gated BEGIN before erase.
- [x] F13 format4/protocol3, matching boot/app and full-region checks/packaging.
- [x] Final PC Release/Debug builds0warnings/0errors and full actual-package
  suites PASS, including boundaries/replay/cancel/CRC/60K/110K regressions.
- [x] F24 split boot/app/standalone clean builds0errors/2license warnings each;
  app38848B CRC15D3F05B, boot end4A90; link/RAM/RFD/BRK audit PASS.
- [x] F13 boot0errors/5warnings (license/vector/record-type), app0errors/3license
  warnings; artifact audit PASS, BIN19160B CRC58FFF3FD, metadata256.
- [x] Actual project tool normal close succeeded during build; final fixed/
  versioned0073 EXEs match compiled Release SHA A6A3F455...0568199D.
- [x] Clean audit24; web194references/version0073; ISP3/Boot3 Mermaid renders.
- [x] Three guide PDFs35pages/22Mermaid diagrams generated directly in docs;
  all-page Poppler review and final corrected-page review PASS. PDF lock closed.
- [ ] Native new-build startup/1366/DPI/TAB/COM and screenshots: NOT RUN.
- [ ] Real128-byte UART/57/58/F13 CAN update/F24 flash/debug/power-loss: MANUAL
  NOT RUN. No explicit flashing/reset/update workflow initiated.
- [x] G16 programming remains BLOCKED; no commit/push/deploy. Historical
  older-version results below are not current compatibility claims.

Detailed changed files, reproduction, warnings and limits:
[tmp/internal/docs/HOST128_METADATA256_VERIFICATION.md](./tmp/internal/docs/HOST128_METADATA256_VERIFICATION.md).

## MD/web sync and build-close policy (2026-10-15)

- [x] Functional delivery requires affected MD/PDF and local web/status/
  screenshot synchronization; publication remains separately authorized.
- [x] User authorizes automatic normal close of this project's running tool;
  full path plus basename guard, no other copy/IDE, no force on timeout.
- [x] Mock fixed/Debug/versioned/foreign/prefix/unknown paths, exit race,
  idempotence, rejection/timeout PASS; no real processes launched/closed.
- [x] Debug one-click entry with CloseRunningTool=false PASS0warnings/0errors;
  clean audit PASS24; no application/firmware/EXE/version/INI change.
- [x] ISP web aligned0072, boot chapter and homepage preparation row updated;
 194 local references/fences/tool-version PASS, ISP3/Boot3 Mermaid render PASS.
- [ ] Actual running-tool graceful close with active COM/worker: NOT RUN.
- [ ] Current HTTP/browser/UI/screenshots: NOT RUN; preview launch command
  blocked by execution policy. Prior desktop pipe failure remains historical.
- [x] No commit/push/deploy, no board traffic/flashing. Existing0072 guide/PDF
  already matches shared console/Misc; no guide content/PDF change needed.

Files/reproduction/evidence: tmp/internal/docs/BUILD_WEB_SYNC_VERIFICATION.md.

## Shared console / bridge UART update0072 (2026-10-15)

- [x] One common BIN/metadata/log/progress pool; CAN/I2C/Misc settings-only TABs;
  no parent scroll;1366/minimum/large-text pure geometry PASS.
- [x] Optional host descriptor/capability; independent HOST destination and
  BUR1 image validation; no raw/CAN/G16 fallback; explicit confirm/RUN.
- [x] F24 boot20KiB/app5000, shared SC, protected last1KiB, explicit stack/RAM
  handoff, far-ROM constants, BRK excluded and RAM RFD/xref audit PASS.
- [x] Final PC Release/Debug0warnings/0errors, both full suites/actual packaged
  image through real C parser/engine PASS;60K/110K/retry/cancel/CRC/power-cut mocks.
- [x] F24 split boot/app and standalone45 clean builds0errors/2 license warnings;
  final app37644B CRC88FDB8C6. F13 artifact checker unchanged PASS; clean audit24.
- [x] User closed locked EXE; fixed delivery replaced/hash verified, INI preserved.
- [x] New guide2-Mermaid/5-page PDF directly in docs; Poppler visual review PASS.
- [ ] Native startup/1366/100%/125%/TAB repaint: NOT RUN, native pipe os error2.
- [ ] Physical initial install, E2 lite halt/continue/stop, IRQ jump, UART flash,
  real cancellation/power loss/recovery: MANUAL NOT RUN. No board programmed.

Full changed-file/reproduction record: tmp/internal/docs/CONSOLE_BRIDGE_VERIFICATION.md.

## TAB/scroll repaint repair (2026-10-15)

- [x] User ghosting screenshot reviewed; source erase-free scroll invalidation
  and missing final parent/child-tree repaint located.
- [x] Tool0.1.0.0071: final-layout erase + all-child/nonclient invalidation;
  parent HWND guard, clamp-only final repaint, remove Invalidate(FALSE).
- [x] Release/Debug x64 builds PASS0warnings/0errors; both full suites PASS;
  clean audit PASS24. Logs tmp/internal/build/diagnostics/repaint_*.
- [ ] Actual ghost-free TAB/scroll/resize/minimize/restore and startup/DPI:
  NOT RUN, Computer Use native pipe unavailable after retry/reset.
- [x] No firmware/wire/BIN/INI/update-gate/web changes or bus traffic.

See tmp/internal/docs/UI_REPAINT_FIX_VERIFICATION.md; software regression PASS does not
close the physical desktop repaint acceptance item.

## Bright identity overview and web preparation (2026-10-15)

- [x] Tool0.1.0.0070 final Release/Debug x64 builds PASS0warnings/0errors;
  both full suites PASS including new live/offline/N/A/stale snapshot tests.
- [x] Host/device/boot/installed-app/loaded-BIN cards have independent sources;
  Transfer boot/app N/A, G16 constants amber and programming still BLOCKED.
- [x] Palette foreground/background contrast6.89..9.47:1, measured card
  detail height and full hover text. Color calculation is not visual DPI QA.
- [x] Clean audit PASS24; no firmware changes, no bus traffic or flashing.
- [x] Identity3-page/one-Mermaid and multi-bus8-page/four-Mermaid PDFs rendered
  with Poppler; all-page visual review PASS. PDFs remain directly under docs.
- [x] Local web194references/7HTTP routes PASS; ISP2/Boot3 Mermaid render PASS;
  user benchmark images copied unchanged. No new repository or publication.
- [ ] New UI startup/capture/100%/125% DPI NOT RUN: Computer Use native pipe
  unavailable after retry/reset. IAB unavailable; browser inventory empty.
- [ ] Boot/App device update/negative physical gates and corrected G16 slave.

See tmp/internal/docs/UI_VISUAL_REFRESH_VERIFICATION.md and local slide screenshot checklist.

## Directory naming follow-up (2026-10-14)

- [x] Canonical name approved: simple_renesas_isp_tool; FirmwareUpgradeTool
  product/project/EXE/INI identities preserved; no new Git repository created.
- [x] Physical folder move completed; active source is simple_renesas_isp_tool.
- [x] Post-move tool0.1.0.0069 Release/Debug x64 builds PASS0warnings/0errors;
  both full test suites PASS. F24 clean build44 PASS,Error0/Warning2 license.
- [x] F13 artifact checker PASS unchanged; all INI options preserved except
  old-folder paths in the other two tools. Clean audit PASS24.
- [x] Renamed multi-bus guide regenerated4 Mermaid/eight pages; Poppler
  rendering and all-page visual review PASS.
- [ ] Post-move startup/UI smoke NOT RUN: desktop native pipe unavailable
  after retry/reset. No physical traffic, firmware programming or reflash.
- [x] Modified Markdown links/fences and workspace clean-script audit PASS24.

## Multi-bus extension gate (current work)

- [x] Active tool and F24 source moved into simple_renesas_isp_tool; product,
      namespace,EXE/INI/log/summary names aligned. Original INI preserved/copied.
      Old root contains only a locked original schematic, with identical new copy.
- [x] Tool0.1.0.0068 Release/Debug x64 builds PASS0warnings/0errors and both full
      suites PASS; logs saved in tmp/internal/build/diagnostics/multibus_*_tmp/internal/build/tests.log.
- [x] Real portable I2C engine and real F24 register/ISR backend mocks; PC
      catalog old/new/multi-port/I2C-only, TX262/RX512, exact EXEC retry,
      cancellation/error/abort; G16 inquiry/signature/geometry/CRC16 PASS.
- [x] Post-move F24 clean build43 PASS,Error0/Warning2 (existing license),
      PROGRAM5FFA/ROM0295/RAM0D67; preserved-user generated IRQ edits only.
- [x] F13 artifact checker PASS unchanged: app1.1.1/19160 bytes/CRC5AAE684B.
- [x] Workspace clean audit PASS24 scripts; no delivery/INI/log deletion.
- [x] Multi-bus4 Mermaid/eight-page PDF and F13 sixteen/22-page PDF regenerated,
      Poppler rendered and visually inspected under docs.
- [ ] New tabs/status/100%/125% DPI startup visual checks: NOT RUN, Computer
      Use native pipe unavailable after retry/reset.
- [!] I2C programming: supplied G16 lacks active READ/auth and safe boot CRC/
      recovery gates. UI stays BLOCKED; no erase/write/app-jump sent. A corrected
      slave contract is required; reference target source remains untouched.
- [ ] Actual F24 Host Link replay/sequence behavior: source-review/build only;
      no directly executed dispatcher harness or hardware trace yet.
- [ ] Physical I2C waveform/reference probe, CAN regression and upgrade faults.

Use `PASS`, `FAIL`, or `NOT RUN` and attach the relevant build log, PC log, or
CAN trace. The full ordered checklist is `DEVELOPMENT_CHECKLIST.md`.

## Documentation gate

- [x] Active documents reference Host Link v2 only.
- [x] No active document links to the removed v1 protocol.
- [x] Host Link and implemented CAN command layouts match PC/F24 definitions;
      automated codec/client tests pass. Physical traces remain open.
- [x] Functional changes update checklist, handoff, and self-check documents.

## PC build gate

- [x] COM catalog displays Windows friendly names without changing canonical
      transport/INI values; catalog tests cover fallback, ordering, duplicate,
      absent/invalid/high-number ports and Unicode. Enumeration is read-only.
- [ ] Latest COM-width/button-wrap Release/Debug startup and 100%/125% DPI
      visual checks: NOT RUN (Computer Use native pipe unavailable).
- [ ] Real COM adapter descriptions/INI refresh and restore: NOT RUN; no COM
      devices are enumerated in the available verification environment.
- [x] Debug and Release compiler outputs use separate directories; only a
      successful Release x64 build copies `tmp\internal\build\release\x64\FirmwareUpgradeTool.exe`
      to the stable `build\FirmwareUpgradeTool.exe` delivery path, so Debug
      clean/rebuild cannot overwrite or remove it.

- [x] x64 Release build succeeds with no new compiler warnings.
- [x] x64 Debug build and earlier Release/Debug startup control-state smokes
      succeed; the new COM-layout smoke remains NOT RUN as listed above.
- [x] Before the COM-name change, 100% DPI layout captures at maximized 1550x830 and restored 1188x675
      confirms measured button captions, metadata rows, fixed update button,
      and prerequisite status do not clip or overlap.
- [x] Before the COM-name change, 125% DPI (120-DPI desktop) loaded/unloaded screenshots confirm all 16
      separate metadata values fit, default to `N/A`, and do not overlap.
- [x] x64 Release executable is native static-MFC code and needs no Python
      runtime.
- [x] Product, EXE, INI, resource, version-symbol, and log names use
      `FirmwareUpgradeTool`; the obsolete INI filename is not read or written.
- [x] Main window starts maximized.
- [x] Implemented host probe, target discovery, and Custom Transfer tests run in
      the worker session and do not pump nested UI messages.
- [x] The CAN Upgrade workflow uses the worker session; error/cancel after
      START performs bounded best-effort target ABORT and CAN_STOP cleanup.

## Host Link gate

- [x] Version is 2 and maximum payload is 96 bytes in PC and F24 code.
- [x] PC CRC known vectors pass; physical F24 parity remains open.
- [x] PC split, concatenated, noisy, and overlapping-SOF tests pass.
- [x] PC accepts 96-byte payloads and rejects 97-byte payloads.
- [x] PC v2 codec passes the version/length, CRC, stream-resynchronization, and
      0/96/97-byte boundary test subset; F24 parity remains open.
- [x] F24 v2 PING/INFO/capabilities/stats dispatcher and replay cache clean-build
      with fixed buffers; hardware request/response testing remains open.
- [x] PC request/response matching uses sequence and command; fake-transport
      tests cover interleaved event and wrong-sequence frames.
- [x] PC timeout retry reuses the byte-identical encoded request and sequence.
- [x] F24 code ignores an identical in-flight retry and replays a completed
      cached response without repeating CAN TX; physical trace remains open.
- [x] F24 code returns `SEQUENCE_ERROR` for same-sequence changed content;
      physical trace remains open.
- [x] PC verifies F24 host identity/capabilities before enabling CAN controls;
      Tool0063/F24 build40 physical 115200 probe PASS in `201510.log`.
- [x] F24 source contains no `printf`/`putchar` call and the clean linker map
      contains no `_printf`/`_putchar`; physical UART capture remains open.
- [x] `RESET_HOST` is one-shot, waits for its binary ACK to leave the F24 UART,
      and uses bounded same-port PC reconnect/probe; mock behavior passes and
      supplied build-34 185719.log confirms 115200 ACK/reconnect PASS. New
      20-MHz firmware/reset at other UART rates remain hardware-pending.
- [x] Background and interleaved asynchronous events are routed through a
      bounded PC queue and decoded for CAN RX/state/error/arbitration loss.

## F24 board gate

- [x] Smart Configurator target and active generated code match schematic MCU
      `R7F124FLJ3AFB_C`; physical board checks remain open.
- [x] Clean CS+ build after the startup-clock/routing correction succeeds:
      Error 0 / Warning 2, firmware `1.0.0.17`, program `0x4C70`, ROM data
      `0x0275`, RAM data `0x0919`.
- [x] CS+ project, `.scfg`, regenerated I/O header, and active generated source
      use `R7F124FLJ3xFB`. CS+ still emits the recorded historical
      device-change/I/O-header notice.
- [x] Regenerated `.scfg` selects X1 20 MHz and HOCO 40-MHz startup. The
      non-generated CS+ preinclude defers X1 start; PLL is stopped. CC-RL
      preprocessed/assembly output confirms CMC=0x51 once and no X1 wait loop.
      Effective CRXD0/CTXD0 are P11/P10: the preinclude overrides regenerated
      PIOR46=1 to 0 in both BSP redirection and CAN driver pin setup.
- [x] Runtime CAN initialization switches 20-MHz X1 /4 x16 to PLL 80 MHz
      and MDIV=1 before UART/timer start; HOCO is used only for startup/fallback.
      CC-RL build and clock timeout/fallback mocks pass.
- [x] Effective nominal/data SJW is 16/6 TQ, not greater than TSEG2. Real
      configuration/header tests preserve 500K/2M; locked-CMC and wrong-divider/
      multiplier cases are covered. Generated driver files remain untouched.
- [x] Final 20-MHz clock/routing CS+ rebuild build 37 PASS (Error 0 / Warning
      2 license); Release/Debug configuration/clock/protocol tests PASS with
      0 warnings/errors; generated-file hashes unchanged (66 files).
- [x] Build40 self-test `192916.log` plus PCAN screenshot confirm physical
      `0x123`/DLC8/counter00..0C and matching input/TX-RAM read-back at 500K.
- [ ] Independently measure the runtime clock; valid frames do not directly
      measure X1/PLL/fCLK frequency or validate clock-failure behavior.
- [ ] After reflashing build 17 or later, CS+ reaches `main` and physical PC
      `Open + Probe Host` returns the F24 identity without timeout.
- [ ] P10/P11/P12 pass physical CAN TX/RX/STB verification.
- [ ] P15/P16 pass physical Host Link UART TX/RX verification.
- [ ] SW1 P32/INTP7 and SW2 P70/INTP8 pass hardware tests.
- [ ] LED1/2/3 P80/P81/P82 show idle, connected, transfer, and error states.
- [x] No ISR calls `printf`, waits for a peripheral, or parses a protocol.
- [x] UART and CAN queues are bounded and report overflow counters in code;
      physical overflow testing remains open.
- [x] CAN initialization, mode transition, TX, abort, and response waits are
      bounded in F24 code; physical timeout/recovery injection remains open.
- [x] Product WDT policy is documented and consistent with project option
      bytes: F24 `0xEF`, F13 boot/app `0xE9`, all with `WDTON=0`. Neither the
      F24 host nor F13 boot/app design uses watchdog-based recovery.

## CAN configuration/filter gate

- [x] UART SET_CAN_CONFIG implements nominal presets 1=250K, 2=500K,
      4=1M in Classical/FD; F24 capability mask is 0x16, data remains 2M.
      Real timing module tests check rate, 80% sample point, SJW, protected
      register writes, all unsupported presets, and clock/mode rejection.
- [x] Nominal dropdown follows advertised host capabilities; changes queue
      worker-owned UART configuration while stopped. Self Test uses the selected
      rate; old 500K-only bridges remain compatible and image/target gates stay.
- [ ] New 0063 dropdown/startup visual smoke: NOT RUN, native Computer Use
      pipe unavailable. Existing layout budgets are unchanged.
- [ ] Physical UART ACK and CAN timing for 250K/500K/1M and FD 2M data.
- [x] Classical CAN 500 kbit/s normal transfer passes: `201510.log`, Tool0063 /
      F24 build40, target RX/TX sequence1..22, F24 RX/TX22/22, zero errors.
- [ ] CAN FD 500 kbit/s nominal / 2 Mbit/s data passes.
- [x] F13 profiles cannot select CAN FD in both UI enablement and worker-side
      validation; physical mode testing remains open.
- [x] Current F13 CAN Upgrade RX metadata/default is `0x701 / 0x7FF` standard data;
      physical filtering verification remains open.
- [x] Zero-mask and accept-all filters are rejected by the UI, generic PC CAN
      client, and F24 host command layer.
- [x] Filter changes require CAN stopped in PC workflow and F24 state
      enforcement; physical filter testing remains open.

## Custom Transfer gate (F13 reference target)

- [x] Target source uses request `0x710`, response `0x711`, exact standard-data
      mask `0x7FF`, and active 500-kbit/s timing; user PCAN/UART evidence and
      `201510.log` confirm the normal communication benchmark.
- [ ] Physical `CLEAR_STATS` passes; not included in the supplied benchmark.
- [x] Physical DEVICE_INFO/PING/ECHO/PATTERN_TEST/GET_STATS pass through PC ->
      F24 -> F13 in `201510.log`; CLEAR_STATS remains separately NOT RUN.
- [x] Active target code no longer retransmits arbitrary received frames.
- [x] CAN initialization and TX/abort waits are bounded; failures and
      warning/passive/bus-off/RX-lost conditions update saturating counters.
- [x] PC Custom Transfer tests pass for CRC-8, request encoding, strict
      response validation, error decoding, exact generic CAN matching, every
      target command workflow, a non-F13 identity, and reserved/malformed
      identity rejection.
- [x] PC worker/UI invokes `DEVICE_INFO`, `PING`, `ECHO`, `PATTERN_TEST`, and
      `GET_STATS` through the generic CAN client with exact filter setup.
- [x] The UI calls this workflow `Custom Transfer`, does not require or validate
      a firmware BIN for it, and retains `RL78_F13_CAN_Transfer` as the first
      separately flashed protocol/reference target.
- [x] The F13 reference target emits configurable main-loop-only `TRACE CT RX/TX`
      lines containing ID, DLC, all frame bytes, sequence, command, and CRC;
      PC detailed trace emits the matching raw frame and decoded CRC result.
- [x] F13 startup reports CAN init result/status and compiled bus/ID/filter
      settings; ISR-captured channel errors are decoded only from the main loop.
      PC Host Link state/error logs decode F24 source bits while preserving raw
      values. User confirmed the transceiver supply cause by successful 5-V
      retest; negative/error-injection validation remains open.
- [ ] Physical boundary, stress, malformed-frame and error-path transfer tests.

## F13 Boot/App gate

- [x] Shared board support uses the verified Transfer X1 4M /2 x16 -> CPU32,
      CAN16/500K baseline. Pre-main setup has no oscillator wait; main-loop
      clock/CAN waits are bounded, inherited PLL is preserved, CAN reset modes
      are established before configuration, and failed initialization gates RX.
- [x] New C90 real-source mocks pass for clock/fallback, wrong CMC/divider,
      RAM/channel/global timeout and boot-to-app peripheral state.
- [ ] Physical paired boot/app1.1.1 clock/CAN, INFO/ENTER/programming, power
      cycle and shared ISR. The successful Transfer benchmark does not close
      these gates. Use 5-V EVDD and newly generated boot_app.hex initially.
- [x] Boot and app active configurations both use Classical CAN 500 kbit/s and
      clean rebuild successfully; physical bus verification remains open.
- [x] Boot/app linker maps use `-OCDTR` and place normal `.dataR/.bss` at
      `0xFE500` or above. Boot Run-to-main resolves to `_boot_main`; app remains
      `_main`. Physical E2 Lite halt/Continue behavior remains open.
- [x] Boot discovery is non-destructive (INFO, then HELLO only for boot state);
      ENTER is a separate explicit action and the app also answers INFO.
- [x] App/boot INFO identifies running state, target, protocol, firmware
      version, and build; physical response capture remains open.
- [x] Boot/app share the exact no-init RAM window `0xFF710-0xFF71F`; the app
      consumes one-shot boot-handoff and successful-update signatures and
      prints configurable `APP_RUNTIME_LOG_TAG` plus launch/update provenance.
- [x] `Start Firmware Update` is a dedicated, fixed-label control. Its adjacent
      status lists missing prerequisites, and it is enabled only for a validated
      F13 bootloader plus a locally valid update image. `Transfer Test` cannot
      invoke the programming handler.
- [x] F13 boot entry depends on explicit app ENTER/RAM marker or invalid image,
      not P13.7. The app's independent P13.7/INTP0 behavior is unchanged.
- [x] Tool validates image magic, format, target, size, version, and CRC before
      START.
- [x] Format 3 accepts a metadata-defined, 4-byte-aligned variable image.
      Tests cover valid 32/60/80/120-KiB files, alternate app start/layout, an
      image larger than 256 KiB, dynamic START/DATA/END, and final CRC position.
- [x] The 96-byte manifest and INFO pages carry image/device MCU, flash layout,
      version/build, CAN capability/ID, size, minimum and integrity fields. The
      UI has no 120-KiB or `0x2000` global limit and `UpdateUi()` reads cached
      validation state instead of recomputing CRC.
- [x] BIN size, calculated/stored CRC and match result are visible before CAN;
      device boot/app metadata is visible after read-only discovery.
- [x] `App Start Override` persists as `Auto` or an explicit matching safety
      assertion; it never relocates an image silently.
- [x] Target source validates START, manifest, flash result, and final CRC;
      physical fault injection remains open.
- [x] PC DATA byte offset advances only after the ACK detail/next address match;
      physical target behavior remains open.
- [x] PC timeout/retry policy matches the documented F13 target protocol;
      physical timing remains open.
- [x] Automated boot/app linker, image, identity, manifest/CRC, and shared-vector
      boundary checks pass. Latest paired 1.1.1 result: boot/FSL end `0x1C85`,
      margin `0x37A` (890 bytes), image `0x4AD8` (19160 bytes), CRC address
      `0x6AD4`, CRC32 `0x5AAE684B`. The checker also guards both CS+ shared
      source registrations and the app preserved-user startup clock hook;
      runtime interrupt dispatch remains open.

## PC diagnostics and mock-test gate

- [x] Error log lines, missing firmware prerequisites, CRC mismatch, and invalid
      metadata use red UI text; ready/valid states use green and non-error
      Custom Transfer guidance uses blue.

- [x] Optional detailed trace records Host Link direction/sequence/command,
      CAN ID/flags/DLC/timestamp/data, decoded F13 fields, retries, and timeouts.
- [x] Normal logging leaves per-frame trace disabled to avoid update-throughput
      impact; the diagnostic option warns that it is slower.
- [x] UTF-8 log recording and exportable session summary include build,
      identity, configuration, image, counters, elapsed time, and final result.
- [x] Mock tests cover normal update, duplicate recovery, timeout, corrupt and
      out-of-order ACK, disconnect, cancellation, END CRC failure, ambiguous
      START cleanup, simulated target FLASH_ERROR/FSL failure, ABORT timeout,
      and invalid-image zero traffic.
- [ ] Shared interrupt redirection passes after update and reset.
- [ ] Power interruption at each update stage leaves the target recoverable.

## End-to-end and release gate

- [x] Standalone bridge-board TX self test is implemented as a cancellable worker
      operation using generic `CAN_TX`: selected Classical 250K/500K/1M,
      default 500K, standard ID `0x123`,
      DLC 8, data `11 22 33 44 55 66 77 XX`, 1000-ms period. `XX` resets to
      `00` on Start and increments per frame; the adjacent `?` tooltip explains
      the CAN-analyzer requirement. Release build passes; physical analyzer
      verification remains open.
- [x] `CAN_TX` accepts both the original 8-byte response and the optional
      40-byte TX diagnostic response. Automated coverage verifies decoding of
      driver input and hardware TX-RAM read-back fields.

- [ ] PC-to-F24 PING/INFO passes at each supported UART rate.
- [x] Normal PC-to-F24-to-F13 Custom Transfer integrity passes (`201510.log`);
      boundary, negative-path, overflow, stress and CLEAR_STATS remain open.
- [ ] Complete F13 update passes and the new app runs after power cycle.
- [ ] Wrong mode/rate/ID/image, disconnect, timeout, overflow, and bus-off tests
      produce specific errors and recover as documented.
- [ ] Tool logs, CAN trace, build logs, and exact firmware versions are archived.
- [x] Source-verified Mermaid draft documents current F13 boot/app/update,
      shared-ISR, PC/F24 host, and transfer-test behavior with hardware-pending
      status stated explicitly.
- [x] Variable-image generation/update and opaque F24 bridge behavior are
      reflected in the 14-diagram Mermaid source; UTF-8, local links, fence
      structure, and Mermaid CLI rendering pass.
- [ ] Final Mermaid diagrams match captured behavior.
- [x] Traditional Chinese PDF guide passes visual page-by-page inspection:
      22 pages, 16 rendered diagrams, embedded Traditional Chinese/monospace
      fonts, benchmark clock/5-V/paired-image and E2 Lite sections, and no clipping, unreadable diagram,
      missing glyph, or blank page. PDF title metadata is readable ASCII.
- [x] F24 TX decoding populates and validates the Renesas `Head.Bits` fields.
      Driver input/RAM read-back diagnostics are implemented; hardware evidence
      disproved the earlier packed-header explanation and identified invalid
      CAN clock configuration. Physical confirmation remains open.

