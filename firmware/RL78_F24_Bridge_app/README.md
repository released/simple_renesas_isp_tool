# F24 bridge app (CAN / I2C / device UART plus self-update entry)

Actual MCU R7F124FLJ3xFB, existing X1 20M -> PLL80 -> CPU/CAN40.
This is the only normal App product. It owns main/custom_func, Host Link
dispatch and CAN services; canonical hardware/drivers/Smart Configurator
sources are shared from ../f24_board. That directory is not a third product.
Portable I2C staging is in ../common; layout/update support in ../f24_bridge_common.

Device UART forwarding uses UART1 TX P74 / RX P75,8-N-1,default115200;
numeric nominal baud300..2000000 is advertised in port bit31. The40MHz
SAU1 divisor must meet the2percent nominal-error guard before acceptance.
Host Link remains independent on UART0 P15/P16. The non-generated
`../f24_board/f24_uart_backend.c` owns SAU1 and PIOR42=1, with interrupt-driven
RX capture and TX. `../common/bridge_uart.c/.h` stages up to512bytes before
one physical EXEC; it has no device metadata or Flash interpretation.
App-local IRQ mapping includes INTST1=3E and INTSR1=40. The existing fixed
Boot table forwards these slots, so adding the App handlers does not rebuild Boot.
See [the optional UART wire contract](../../docs/HOST_LINK_PROTOCOL_V2.md#17-optional-device-uart-forwarding).
IICA0 remains SCL P62/SDA P63;port rates advertise100/400kHz(default100).
Selected timing is retained after abort/recreate. Verify connector wiring, common ground,
voltage and pull-ups before use; forwarding models are not electrical tests.

CAN_BURST_WAIT_RX22 / feature bit12 sends up to10 standard
Classical frames per128-byte request, one matching reply after the last TX.
The nonblocking progression is shared with HAL fault tests. No F13 protocol,
offset, CRC or Flash knowledge exists here. Install App through Misc115200,
RUN explicitly, then Tool reconnects and confirms live App identity/metadata.

GET_STATS reports UART/CAN loss/error counters and queue/timing peaks.
Tool computes per-update counter deltas from fresh before/after snapshots;
peaks cover boot/CLEAR_STATS lifetime, not only the current update.
See the [confirmation and report guide](../../docs/UPDATE_VALIDATION_zh-TW.md).

Use [Host App engineering](../../docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md#host-app-engineering)
for the C-header metadata/IRQ editing steps and portable parser APIs.

App start0x5000, capacity235KiB, last1KiB protected. Only after reference SC
regeneration run scripts/prepare_f24_bridge_projects.ps1 -Kind app. Routine
scripts/build_f24_bridge.ps1 -Kind app performs a clean build and artifact
audit without reference-project preparation. CS+ pre/post-build run automatically.

Each app owns app_irq_handlers.h and app_image_config.h. Pre-build runs
scripts/prepare_f24_app_irq.ps1 -AppProject "." to validate registered normal
ISRs and generate bridge_vectors.asm plus f24_app_identity.generated.h.
Post-build also compares the actual compiled release-identity record with
app_image_config.h. Regenerating headers cannot relabel already linked code.
Never edit generated outputs. App ISR pragmas have no vect=; all return RETI.
Reserved/BRK vector keys, ordinary/static/unregistered handlers are rejected.

Build an independent compatible app with:

    scripts/build_f24_bridge.ps1 -Kind app -AppProject <app-folder>

The folder must contain exactly one MTPJ and retain this split-app startup,
linker/RAM and BRIDGE_SPLIT_APP contract. When copying to another directory,
fix source/include/library/preinclude paths and the pre/post-build script paths.
Do not run the canonical reference-project prepare script for a custom app.
The app-only build/generator does not change boot source/project/firmware.
Post-build uses this app's project stem, local identity, slot mapping and real
RETI/RETB epilogues in addition to existing bounds/CRC/RAM audits.

For reference SC regeneration use prepare_f24_bridge_projects.ps1 -Kind app;
it retains local C headers, regenerates only app projections/MTPJ and leaves boot
untouched. Change the IRQ X-macro explicitly when enabling/disabling a driver.
See the app-local section in docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md.

- *_bridge_update.bin: load this HOST APP file in tool Misc.
- *_flash.mot: initial app installation with a matching boot via debugger.
- Raw .abs/.mot: debug/compiler outputs, NOT standalone installation images.
  The device file emits an unbound low-vector placeholder; packaging removes
  all low-vector/options/debug records and supplies the BUR1 manifest/CRC.

Post-build derives image length from linked code/data, pads holesFF, writes
256-byte metadata region at offset100h (contract3 BUR1 core128, reserved128FF),
adds a four-byte CRC32 tail. No fixed-size define
for60K/110K is required. The64-entry app IRQ table occupies offsets0..FFh;
code starts at offset200h. All live geometry must match boot before BEGIN.

INFO reports actual installed flash CRC/manifest; ENTER only arms a reset
after its binary ACK is successfully queued and physically transmitted.
The runtime requires BRIDGE_SPLIT_APP; no standalone build/identity fallback
remains. Install matching Boot once, then use Misc for App-only updates.

Hardware validation remains pending; see the shared guide and HANDOFF.
Metadata geometry/identity belongs to f24_bridge_common/image_metadata_config.h;
its profile and common bridge_image_metadata.c/.h are independently compiled.
The parser receives byte extents and has no CAN/UART/Flash-driver dependency.
boot_app_handoff.h owns no-init RAM, separate from serialized metadata.
For recovery, hold SW1/P32 LOW during reset/power-up, release after Boot starts,
then use Misc115200 to update App and explicitly RUN.
