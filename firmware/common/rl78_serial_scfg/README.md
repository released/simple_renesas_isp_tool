# RL78 serial Smart Configurator profiles

These six initialization profiles cover F24/G16/G22 UART and IICA hardware.
Xmodem uses its MCU's UART profile; its wire protocol remains independent.
Project-local SCFG files are editable inputs, not normal-build mirror targets.

| Profile | Origin / device contract |
|---|---|
| F24 | Same-device serial/CAN and IICA slave reference configurations; R7F124FPJ5xFB, X1 4MHz / PLL80 / CPU40 |
| G16 | Same-device IIC-master/IICA-slave reference; R5F121BCxFP, HOCO16 |
| G22 | G16 common component schema with installed G22 device-specific clock, package pins and generated IRQ resources; R7F102GGExFB, HOCO32 |

All six were exercised with the installed vendor headless generator before
project rollout. They contain only BSP, UART0, TAU0_1 and the selected UART1
or IICA0. Device-specific pins are not inferred from another MCU's package.
SC IICA address is the SVA register representation: seven-bit20h becomes40h
(decimal64 in the SC field). Tool and C transport headers keep seven-bit form.

`scripts/prepare_rl78_serial_scfg.ps1 -Project <existing project>` installs the
CS+ association and local validator; it only seeds a missing configuration.
`-Initialize` explicitly replaces a project's SCFG with the applicable profile.
It is a maintainer initialization operation, never a normal pre-build action.
`-Generate` additionally runs the installed vendor generator,for explicit
initialization/scaffold workflows; it does not install tools or run on Rebuild.
`-CheckOnly` validates current project inputs and its generated projection.

Each Boot carries an identical local `validate_scfg.ps1`. Normal firmware
Rebuild does not depend on this Tool checkout or a reference project. App and
Transfer retain their existing paired sibling Boot dependency. The validator
checks actual SC register output and emits a project-local C header, which is
preincluded by CC-RL. It does not run SC, edit a SCFG, rewrite a paired Boot or
add generated callback/vector/startup implementations to the link.

Protocol queues, mailbox completion, Flash, reset authorization and vector
ownership remain in the existing runtime. The generated header carries only
the checked board initialization values. A port/clock architecture change
requires adapting the board code and validator together.

See the canonical serial engineering guide for the reader workflow. Native
tests and SC generation/build checks are separate from physical acceptance.
