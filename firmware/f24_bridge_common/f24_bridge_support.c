#include "platform.h"
#include "f24_bridge_support.h"
#include <string.h>
uint8_t F24Bridge_Read(void *context, uint32_t address) {
    (void)context; return *(const uint8_t __far *)address;
}
void F24Bridge_ResetToBoot(void) {
    BSP_DI(); *(volatile uint32_t __far *)F24_BRIDGE_COOKIE_ADDRESS=F24_BRIDGE_BOOT_COOKIE;
    F24Bridge_Reset();
}
void F24Bridge_Reset(void) {
    BSP_DI(); IAWCTL|=0x80U; *(volatile uint8_t __far *)0UL=0U;
    for (;;) { BSP_NOP(); }
}
