# RL78/F24 Host Bring-up and Smart Configurator Guide

Status: source and clean-build verified; physical board checks remain open.

## Active hardware baseline

| Function | Pin / generated interface |
| --- | --- |
| CAN TX / RX / transceiver standby | P10 / P11 / P12 |
| Host Link UART TX / RX | P15 / P16, `Config_UART0` |
| SW1 / SW2 | P32/INTP7 / P70/INTP8 |
| LED1 / LED2 / LED3 | P80 / P81 / P82, active high |
| 1 ms system tick | `Config_TAU0_1` |
| IICA0 master | SCL P62 / SDA P63;100kHz; released inputs until configured |
| Reset-to-main startup clock | Internal HOCO 40 MHz; X1/PLL start deferred |
| Runtime CAN clock | X1/HXT 20 MHz /4 x16 -> PLL 80 MHz, MDIV=1 -> fCLK/fCAN 40 MHz |

The fitted MCU is `R7F124FLJ3AFB_C`. The active CS+ device, `.scfg`,
`iodefine.h`, and generated source headers identify `R7F124FLJ3xFB`.

The user regenerated Smart Configurator for a 20-MHz X1 crystal. Reset-to-main
uses the internal 40-MHz HOCO as `fMAIN`, `fMP`, and `fCLK`. CS+ preincludes
`f24_board_config.h` for every C unit: generated configuration is loaded first,
then `BSP_CFG_HISYSCLK_OPERATION=1` defers X1 startup. X1 and PLL are not
started before `main`; the generated driver files are not hand-edited.
`hardware_init()` then calls the hand-maintained `f24_can_clock.c` before
starting UART0 or TAU0_1. It starts the board's 20-MHz crystal, selects X1,
locks PLL at 80 MHz, and sets MDIV to 1 before selecting PLL. Both the working
PLL configuration and the HOCO fallback have a 40-MHz CPU clock, preserving
the generated UART divisor and 1-ms timer. The board must actually use a
20-MHz crystal; do not flash this variant onto the schematic's original 4-MHz
board without changing the crystal.

CMC is write-once after reset. BSP's first write is `0x51` in the current
configuration: X1 mode, AMPH=1, and the preserved low-speed-clock selection.
Runtime checks the X1/AMPH bits but never rewrites CMC. The old runtime rewrite
after BSP had locked CMC at 0 caused build-34 X1 timeout (`185719.log`).

The user's subsequent regeneration now correctly selects PIOR46=0 (P11/P10).
The board preinclude retains this value for both BSP and CAN driver to protect
the physical transceiver route against future configuration regressions.

The valid 20-MHz PLL combination is input /4 (5 MHz), multiplication x16,
no output division, and MDIV=1. CKSEL divider bits are set atomically to `111`;
PLLCTL is `0xE2` before PLLON, `0xE7` after selection. Lock wait is
1024/20 MHz = 51.2 us, exceeding the 40-us minimum. Failure cleanup stops PLL
before changing MCM0 back to HOCO, as required by the manual.

The CAN module requires `fCLK=fMP/2 >= fCAN` and prohibits HOCO as the CAN
communication clock. See the [Renesas Hardware Manual, chapter 18.17](https://www.renesas.com/en/document/mah/rl78f23-24-users-manual-hardware).
The previous HOCO/MDIV=0 configuration violated this requirement: build-30
hardware logs show correct driver inputs but invalid/zero TX-RAM read-back,
with analyzer frames at ID 0, DLC 0. An error-free analyzer trace does not prove
that CAN message RAM accesses are valid.

Oscillator and PLL waits are bounded. An X1/PLL timeout restores HOCO and
leaves CAN uninitialized; PC probe remains available for this failure mode.
`SET_CAN_CONFIG` returns a clock-stage diagnostic and the Tool explains the
failure. Hardware switch/fallback verification remains pending.

WDT is not used by the F24 host product design: link option byte 0 is `0xEF`
(`WDTON=0`) and `BSP_CFG_WDT_REFRESH_ENABLE` is 0. Runtime recovery uses
bounded UART/CAN state machines, explicit errors, and explicit reset.

## Preserved generated callbacks

After every Smart Configurator generation, confirm these user-code hooks still
exist before building:

- UART TX completion calls `HostLinkUart_TxCompleteFromIsr()`.
- UART RX and software-overrun paths call
  `HostLinkUart_RxByteFromIsr()`; UART errors call
  `HostLinkUart_RxErrorFromIsr()`.
- TAU channel callback calls `Timer_1ms_IRQ()`.
- INTP7 and INTP8 callbacks call `Button1_Process_in_IRQ()` and
  `Button2_Process_in_IRQ()`.
- ISRs only capture bytes, flags, results, or counters. They do not parse Host
  Link or wait for a peripheral.
- P15/P16 are a dedicated binary Host Link. The hand-maintained F24 source has
  no `printf`/`putchar` call, and the clean-build map has no `_printf` or
  `_putchar` symbol. Do not add banners or text diagnostics after regeneration.

`hardware_init()` performs bounded clock/CAN initialization before starting
UART0, TAU0_1, INTP7, and INTP8. It keeps P12 high (standby) until Host Link
`CAN_START`.
`loop()` services buttons, LEDs, Host Link parsing, UART TX, generic CAN
transactions, and asynchronous CAN/error events.

`RESET_HOST` is not an unannounced reset: the F24 first queues a successful
binary response, waits until the UART TX queue and peripheral are idle, and
then invokes the existing software reset. `FirmwareUpgradeTool` closes the old COM
session and performs a bounded reopen plus identity/capability probe on the
same port and baud rate.

## CAN presets and filters

- Classical CAN: nominal preset 1=250K, 2=500K (default), 4=1M; data unused.
- CAN FD: the same nominal presets, fixed data preset 1=2M.
- The generated timing uses an 80-TQ nominal bit at 40 MHz for 500 kbit/s and
  a 20-TQ data bit at 40 MHz for 2 Mbit/s.
- The generated sample's SJW 17/7 TQ exceeds its TSEG2 16/6 TQ. The board
  preinclude corrects effective SJW to 16/6 TQ, including derived register
  values NSJW=15 and DSJW=5. BRP, segments, sample points and bitrates do not
  change. Keep this override after regeneration, or set valid values in SC.
- The initial safe filter is standard data `0x701 / 0x7FF`.
- Runtime filter index 0 may be standard or extended, but an enabled zero mask
  and out-of-range ID/mask are rejected.
- Configuration and filter changes are accepted only while CAN is stopped.
- F13 profiles use Classical CAN only. Selecting CAN FD is rejected by the PC
  workflow before a Host Link command is sent.

## LED states

| State | Source behavior |
| --- | --- |
| Idle | LED1 toggles every 500 ms |
| Host connected | LED1 steady on |
| Transfer start | LED1 + LED2 steady on |
| Transfer active | LED1 steady, LED2 toggles every 125 ms |
| Error | LED3 toggles every 250 ms |

The source-level error-clear policy is: reset enters Idle; any new valid Host
Link request enters Host connected before dispatch; a CAN error returned by
that request restores Error. `CLEAR_STATS` clears counters after its successful
response but does not otherwise change CAN state. Confirm that this policy is
usable and visible on the physical board before release.

These are source-level mappings. Polarity, visibility, and state transitions
must still be checked on the physical host board.

## Regeneration procedure

1. Close running builds. The only CS+ products are
   `firmware/RL78_F24_Bridge_boot` and `firmware/RL78_F24_Bridge_app`.
   Shared `firmware/f24_board` is not a flashable project. After relocating the
   workspace run `scripts/prepare_f24_bridge_projects.ps1 -Kind app` to refresh
   the SC output-folder path. Open the App MTPJ and its shared
   `firmware/f24_board/f24_board.scfg`; verify output is `f24_board/src/smc_gen`,
   never either project's `generated_projection` folder.
2. Confirm target `R7F124FLJ3xFB`, UART0, TAU0_1, INTP7, INTP8, and the current
   CAN FD component settings. Select X1 crystal mode at 20 MHz, HOCO 40 MHz
   as fMAIN/fMP/fCLK, and PLL stopped. Keep CS+ CC-RL Compile Options ->
   Preprocess -> Include files at head of compiling units set to
   `f24_board_config.h`. It defers X1 start even though SC enables X1 mode.
   The bounded runtime CAN clock switch is implemented in
   `f24_can_clock.c`, outside generated code. Do not remove that switch;
   CRXD0/CTXD0 must be P11/P10 (`PIOR46=0`).
3. Generate code with Smart Configurator. Do not hand-edit generated
   implementation sections; keep glue in user sections or hand-maintained
   modules.
4. Recheck every callback listed above and ensure `F24_BoardIo_Init()` still
   owns LED/STB/IICA0 policy.
5. Regenerate the App's derived no-vect/startup projections and source entries,
   then run a clean rebuild:

   ```powershell
   .\scripts\prepare_f24_bridge_projects.ps1 -Kind app
   .\scripts\build_f24_bridge.ps1 -Kind app
   ```

6. If Boot's own UART/timer/port/startup configuration changes, explicitly run
   the same commands with `-Kind boot`; App-only changes do not rebuild Boot.
   The wrapper parses the final CS+ summary and runs packaging guards. A zero
   CS+ exit code alone is not proof of success. Prepare reads each owned MTPJ,
   preserves compiler/linker/build hooks and App JSON, and reconciles only SC
   source registrations/projections. It no longer depends on a third project.

The shared configuration/source move is clean-build and byte-equivalence
verified. Opening/regenerating SC at the new location remains a manual GUI gate;
inspect preserved callbacks and rerun preparation before compiling regenerated code.
See [shared board source ownership](../firmware/f24_board/README.md).

The final clock/routing correction build is recorded in HANDOFF.md with
Error 0 / Warning 2. Both warnings are expired CC-RL license warnings. CS+ also
prints a device-change/I/O-header notice due to historical FPJ5 records in
project metadata even though the active device and generated code are FLJ3;
resolve that IDE notice before release if CS+ continues to request generation.

## Host probe startup troubleshooting

If CS+ remains running before reaching `main` and `Open + Probe Host` times
out, first inspect the reset-to-main clock path. The previous configuration
started X1 at 4 MHz and contained unbounded generated waits for X1 stabilization
and PLL lock before `main`; the same-board SelfTest reaches `main` using HOCO.
The active reset-to-main configuration therefore uses HOCO 40 MHz. The runtime
X1/PLL waits execute after `main` and have fixed polling limits. Do not add UART
text prints because P15/P16 carry the binary protocol. Confirm the newly
flashed image reports the recorded 20-MHz build. First confirm CMC's X1 mode
bits: 0 means startup locked the pins as ports, not a proved crystal fault.
A clock error at X1 stabilization with correct CMC mode requires checking
the 20-MHz oscillator and its P121/P122 connections;
a PLL-lock error requires checking the actual external frequency/PLL settings.

## Runtime bitrate selection by UART

PC-to-bridge commands use binary USB-UART, not the CAN bus. No initial 500K
CAN handshake is required to reconfigure the bridge. The capability-aware
UI requires CAN stopped before enabling settings, sends SET_CAN_CONFIG with preset 1
(250K), 2 (500K), or 4 (1M), then
reapplies the filter and starts CAN when a workflow begins. Complete/cancel
pending transactions first.
The current firmware advertises nominal mask 0x16 and data mask 0x02. Dropdown
changes immediately queue UART configuration without starting CAN; discovery
and Self Test reapply the chosen settings before CAN_START. Older bridges with
nominal mask 0x04 offer 500K only. The existing INI preserves the selection;
new/default settings and MCU reset use 500K.
Every device on the CAN bus must use the same nominal bitrate. Changing the
bridge does not automatically change the F13 device's bitrate.

At fCAN=40 MHz, the hand-maintained f24_can_timing module applies:

| Nominal | BRP register / divisor | SS / TSEG1 / TSEG2 / SJW | C0NCFGH / C0NCFGL |
| --- | --- | --- | --- |
| 250K | 1 / 2 | 1 / 63 / 16 / 16 | 0x1E7C / 0x3C01 |
| 500K | 0 / 1 | 1 / 63 / 16 / 16 | 0x1E7C / 0x3C00 |
| 1M | 0 / 1 | 1 / 31 / 8 / 8 | 0x0E3C / 0x1C00 |

All nominal sample points are 80%. Data timing remains the generated 2M
baseline with corrected SJW 6. Invalid presets, missing clock, and non-reset
controller modes are rejected without timing writes. CAN_START reapplies the
selected timing after R_CAN_SetConfig, preventing an unintended return to 500K.

## Physical bring-up gate

Do not infer these results from a clean build. First confirm that CS+ reaches
`main` and that `Open + Probe Host` returns host identity without retries.
Capture tool logs and a CAN trace for UART host probe at each supported baud,
Classical 250K/500K/1M, CAN FD with each nominal rate and 2M data,
exact-filter rejection, both buttons, every LED state, disconnect,
timeout, bus-off, and queue-overflow recovery.
