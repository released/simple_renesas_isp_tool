#include "platform.h"
#include "r_cg_macrodriver.h"
#include "Config_PORT.h"
#include "Config_UART0.h"
#include "Config_TAU0_1.h"
#include "r_cg_sau_common.h"
#include "r_cg_tau_common.h"
#include "f24_bridge_support.h"
static volatile uint16_t g_milliseconds;
/* Minimal boot peripherals; do not initialize CAN/IICA or switch to X1. */
void R_Systeminit(void) {
    *(volatile uint8_t __far *)F24_BRIDGE_DISPATCH_ADDRESS=F24_BRIDGE_BOOT_OWNER;
    R_Config_PORT_Create(); R_SAU0_Create(); R_TAU0_Create();
}
void tick_counter(void) { ++g_milliseconds; }
unsigned int get_tick(void) { return g_milliseconds; }
void set_TIMER_PERIOD_1000MS(void) { }
void reset_TIMER_PERIOD_1000MS(void) { }
void Timer_1ms_IRQ(void) { ++g_milliseconds; }
