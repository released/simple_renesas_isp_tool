/* Local image profile: constants only, no platform/register dependency. */
#include "image_metadata_profile.h"
#include "image_metadata_config.h"
#include <string.h>
typedef char FamilyFitsMetadata[(sizeof(F24_BRIDGE_FAMILY) <= 13U) ? 1 : -1];
typedef char PartFitsMetadata[(sizeof(F24_BRIDGE_PART) <= 17U) ? 1 : -1];
void F24Bridge_Info(bu_info_t *d, uint8_t mode) {
    memset(d,0,sizeof(*d)); d->mode=mode; d->max_data=BU_MAX_DATA;
    d->board_id=F24_BRIDGE_BOARD_ID; d->boot_version=F24_BRIDGE_BOOT_VERSION;
    d->boot_build=F24_BRIDGE_BUILD; d->app_version=0xFFFFFFFFUL;
    d->app_build=0xFFFFFFFFUL; d->installed_size=0xFFFFFFFFUL;
    d->app_start=F24_BRIDGE_APP_START; d->boot_size=F24_BRIDGE_APP_START;
    d->capacity=F24_BRIDGE_APP_LIMIT-F24_BRIDGE_APP_START;
    d->flash_size=F24_BRIDGE_FLASH_SIZE; d->erase_size=F24_BRIDGE_ERASE_SIZE;
    d->minimum_size=F24_BRIDGE_MINIMUM_SIZE; d->uart_baud=F24_BRIDGE_UART_BAUD;
    d->irq_abi=F24_BRIDGE_IRQ_ABI;
    memcpy(d->part,F24_BRIDGE_PART,sizeof(F24_BRIDGE_PART)-1U);
    memcpy(d->family,F24_BRIDGE_FAMILY,sizeof(F24_BRIDGE_FAMILY)-1U);
}

