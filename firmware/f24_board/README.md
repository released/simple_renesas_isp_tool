# Shared F24 board sources (not a firmware product)

Only `../RL78_F24_Bridge_boot` and `../RL78_F24_Bridge_app` are buildable CS+
projects. This directory has no MTPJ, application entry point or flash output.
Do not create a third standalone bridge firmware from it.

| Owner | Contents |
| --- | --- |
| `f24_board/` | Board configuration, clock/timing, UART/I2C backends, canonical SC configuration and generated sources |
| `RL78_F24_Bridge_boot/` | UART recovery/update runtime, flash driver integration and physical IRQ dispatch |
| `RL78_F24_Bridge_app/` | Normal bridge runtime, Host Link command dispatch, CAN service and app-local IRQ/identity manifests |
| `../common/` | Portable wire limits, transport interfaces and I2C staging engine |
| `../f24_bridge_common/` | Shared boot/app layout, image contract and update support |

`f24_board.scfg` and `src/smc_gen/` are the sole Smart Configurator inputs and
outputs. Both projects point to this configuration. After relocating the
workspace, run `scripts/prepare_f24_bridge_projects.ps1 -Kind app` from the tool
root before opening Smart Configurator; this refreshes its output-folder path.
Open SC through the App project and check that output remains in this directory,
not in either project's `generated_projection` directory.

After SC generation, inspect preserved UART/timer/button/IICA callbacks, then
run `prepare_f24_bridge_projects.ps1 -Kind app` and rebuild App. Prepare Boot
separately with `-Kind boot` only when its own hardware/startup configuration
changes. The default `all` intentionally prepares both, not a routine App-only
update. GUI regeneration at the new location still needs a manual SC check;
the checked-in configuration and clean CC-RL builds are verified.

The preparation script reads each project's existing MTPJ; it never clones a
third project or resets project-local compiler/linker/build-hook settings.
It reconciles registered generated sources, derives no-physical-vector ISR and
startup projections, and preserves App's JSON mapping/identity. Do not edit
projections directly or compile canonical vect-bearing sources alongside them.
New App ISRs still require an explicit `app_irq_handlers.json` entry.

`f24_runtime_hooks.h` is the small shared UART/timer callback interface.
Boot's `boot_platform.c` and App's own runtime implement it independently;
shared drivers must not include App's `inc_main.h` or depend on App source files.

Start with [F24 bring-up](../../docs/F24_BRINGUP.md) and the
[Boot/App porting guide](../../docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md).
