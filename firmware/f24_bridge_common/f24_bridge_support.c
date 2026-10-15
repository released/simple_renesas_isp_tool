#include "platform.h"
#include "f24_bridge_support.h"
#include <string.h>
uint8_t F24Bridge_Read(void *context, uint32_t address) {
    (void)context; return *(const uint8_t __far *)address;
}
void F24Bridge_Info(bu_info_t *d, uint8_t mode) {
    memset(d,0,sizeof(*d)); d->mode=mode; d->max_data=BU_MAX_DATA;
    d->board_id=F24_BRIDGE_BOARD_ID; d->boot_version=F24_BRIDGE_BOOT_VERSION;
    d->boot_build=F24_BRIDGE_BUILD; d->app_version=0xFFFFFFFFUL;
    d->app_build=0xFFFFFFFFUL; d->installed_size=0xFFFFFFFFUL;
    d->app_start=F24_BRIDGE_APP_START; d->boot_size=F24_BRIDGE_APP_START;
    d->capacity=F24_BRIDGE_APP_LIMIT-F24_BRIDGE_APP_START;
    d->flash_size=F24_BRIDGE_FLASH_SIZE; d->erase_size=F24_BRIDGE_ERASE_SIZE;
    d->minimum_size=F24_BRIDGE_MINIMUM_SIZE; d->uart_baud=F24_BRIDGE_UART_BAUD;
    d->irq_abi=IP_F24_IRQ_ABI;
    memcpy(d->part,"R7F124FLJ3xFB",14U); memcpy(d->family,"RL78/F24",9U);
}
void F24Bridge_ResetToBoot(void) {
    BSP_DI(); *(volatile uint32_t __far *)F24_BRIDGE_COOKIE_ADDRESS=F24_BRIDGE_BOOT_COOKIE;
    F24Bridge_Reset();
}
void F24Bridge_Reset(void) {
    BSP_DI(); IAWCTL|=0x80U; *(volatile uint8_t __far *)0UL=0U;
    for (;;) { BSP_NOP(); }
}
