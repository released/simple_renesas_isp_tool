# F24 bridge boot (UART recovery / app update)

Actual MCU R7F124FLJ3xFB, HOCO40 UART115200, boot20KiB, app0x5000.
This is the only Boot product. Canonical board/driver/Smart Configurator
sources live in ../f24_board, which is shared source, not another firmware.
Normal bridge functionality belongs to ../RL78_F24_Bridge_app.
Do not regenerate Smart Configurator into this project's projection folder.

Start with [RL78 Boot/App porting](../../docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md)
for reusable IRQ, startup/linker/RAM, build and installation requirements.

Run scripts/prepare_f24_bridge_projects.ps1 -Kind boot for intentional boot SC changes;
then scripts/build_f24_bridge.ps1 -Kind boot. The MTPJ owns its build settings;
prepare updates shared-source registrations and startup/IRQ projections only.
Projections are reproducible, not hand-edit targets. Layout is centralized in
../f24_bridge_common/f24_bridge_layout.h. RFD vendor source is copied unchanged
with its Renesas license/disclaimers from the supplied Type02 reference.

For app-only work do not regenerate/rebuild boot: each app owns its JSON
IRQ map and identity, with scripts/build_f24_bridge.ps1 -Kind app -AppProject
<folder>. The fixed normal-vector interface does not depend on app ISR names.
Use prepare_f24_bridge_projects.ps1 -Kind boot only for intentional boot
configuration changes; omitting Kind still regenerates both reference projects.

Initial Boot installation requires a debugger/programmer. Either install Boot
MOT plus matching App *_flash.mot, or install Boot only, then use tool Misc at
115200 to load matching *_bridge_update.bin, validate/update and explicitly Run
Validated Bridge App. With no valid App, Boot stays in recovery. Misc never
updates Boot itself.
SW1 forces recovery; invalid manifest/CRC always stays boot.
No printf on binary UART. Only app blocks can be erased/written; no boot,
option bytes, data flash or last1KiB access is exposed by update commands.

Current Host Link payload is128 bytes (frame142), bridge update contract3.
The metadata region is256 bytes (128-byte core plus128 reservedFF), staged and
read in64-byte pages. BEGIN validates the complete staged region before erase.
Use the matching Boot/App/Tool contracts for installation and communication.

Read-only command0x59 reports why Boot is staying: missing/invalid App,
metadata/contract/CRC rejection, SW1, Flash failure, ENTER or completed update.
Tool logs the reason during Misc discovery. See the
[update confirmation guide](../../docs/UPDATE_VALIDATION_zh-TW.md).

Boot uses a bounded4KiB startup stack, noinit handoff at FF700/FF710,
hardware IRQ dispatch and full image CRC before jump. BRK007E is excluded
from physical IRQ forwarding; RETB differs from RETI. RFD plus flash-mode
driver code/critical hooks execute in RAM with IRQs disabled. Timeout resets
from RAM into recovery. Build/link checks are not physical proof.

Physical IRQ handoff, UART timing, supply, erase/write, cancellation/power loss,
and debug/standalone reset require a controlled lab verification before release.
See ../../docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md and its PDF directly in docs.
