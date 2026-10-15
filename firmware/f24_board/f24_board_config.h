#ifndef F24_BOARD_CONFIG_H
#define F24_BOARD_CONFIG_H

/* CS+ preincludes this file for every C compilation unit. Keep generated
 * configuration intact: load it first, then apply the board runtime policy.
 * The board must actually have a 20 MHz crystal on X1/X2. */
#include "src/smc_gen/r_config/r_bsp_config.h"
#include "src/smc_gen/r_config/r_rscanfd_rl78_config.h"

#if (BSP_CFG_HISYSCLK_SOURCE != 1) || (BSP_CFG_FMX_HZ != 20000000)
#error Configure_Smart_Configurator_for_a_20_MHz_X1_crystal
#endif

/* BSP writes CMC once, selecting X1 mode with AMPH=1. Do not start X1 or
 * enter its unbounded generated wait before the debugger's main breakpoint.
 * f24_can_clock.c starts X1 and PLL with bounded waits after main. */
#undef BSP_CFG_HISYSCLK_OPERATION
#define BSP_CFG_HISYSCLK_OPERATION (1)

/* The host transceiver is wired to P10/P11, not the P72/P73 alternative.
 * Apply the same choice to BSP redirection and CAN driver pin setup. */
#undef BSP_CFG_PIOR46
#define BSP_CFG_PIOR46 (0)

/* The supplied FD baseline has SJW greater than TSEG2. Correct SJW only;
 * preserve BRP and both segments so nominal/data rates remain 500K/2M.
 * Derived CAN_CFG_NSJW/DSJW macros expand these effective values. */
#undef CAN_CFG_NMNL_SJW
#define CAN_CFG_NMNL_SJW (16)
#undef CAN_CFG_DATA_SJW
#define CAN_CFG_DATA_SJW (6)

#if (CAN_CFG_NMNL_TSEG1 <= CAN_CFG_NMNL_TSEG2) || \
    (CAN_CFG_NMNL_TSEG2 < CAN_CFG_NMNL_SJW) || \
    (CAN_CFG_DATA_TSEG1 < CAN_CFG_DATA_TSEG2) || \
    (CAN_CFG_DATA_TSEG2 < CAN_CFG_DATA_SJW)
#error Invalid_F24_CAN_bit_timing
#endif

#endif
