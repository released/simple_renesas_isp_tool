# F24 bridge app (CAN / I2C plus UART self-update entry)

Actual MCU R7F124FLJ3xFB, existing X1 20M -> PLL80 -> CPU/CAN40.
This is the only normal App product. It owns main/custom_func, Host Link
dispatch and CAN services; canonical hardware/drivers/Smart Configurator
sources are shared from ../f24_board. That directory is not a third product.
Portable I2C staging is in ../common; layout/update support in ../f24_bridge_common.

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
