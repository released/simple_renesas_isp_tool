#include "bridge_update_contract.h"
#include <string.h>
uint16_t Bu_Read16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
uint32_t Bu_Read32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
void Bu_Write16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
void Bu_Write32(uint8_t *p, uint32_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); p[2]=(uint8_t)(n>>16); p[3]=(uint8_t)(n>>24); }
static uint8_t text_field(const uint8_t *p, uint8_t n, char *out) {
    uint8_t i, ended;
    ended = 0U;
    if (p[0] == 0U) return 0U;
    for (i=0U; i<n; ++i) {
        if (p[i] == 0U) ended = 1U;
        else if (ended || p[i] < 0x20U || p[i] > 0x7EU) return 0U;
        out[i] = (char)p[i];
    }
    out[n] = '\0'; return 1U;
}
static uint8_t geometry(uint32_t start, uint32_t capacity, uint32_t flash,
                        uint32_t boot, uint32_t erase, uint32_t minimum) {
    if (!erase || erase%4UL || start < boot || !boot || start % erase || capacity % erase ||
        flash <= start || !capacity || capacity > flash-start ||
        minimum < BU_MANIFEST_OFFSET + BU_METADATA_REGION_SIZE + 4UL ||
        minimum > capacity || minimum % 4UL) return 0U;
    return 1U;
}
uint8_t Bu_ParseManifest(const uint8_t *p, bu_manifest_t *m) {
    if (Bu_Read32(p)!=BU_IMAGE_MAGIC || p[4]!=BU_PROTOCOL || p[5]!=BU_MANIFEST_SIZE ||
        p[6]!=2U || p[7]!=1U || Bu_Read16(p+10)!=2U || Bu_Read32(p+84) ||
        Bu_Read32(p+88) || Bu_Read32(p+92)) return 0U;
    memset(m,0,sizeof(*m));
    m->board_id=Bu_Read16(p+8); m->image_size=Bu_Read32(p+12);
    m->version=Bu_Read32(p+16); m->build=Bu_Read32(p+20);
    m->app_start=Bu_Read32(p+24); m->capacity=Bu_Read32(p+28);
    m->flash_size=Bu_Read32(p+32); m->boot_size=Bu_Read32(p+36);
    m->erase_size=Bu_Read32(p+40); m->minimum_size=Bu_Read32(p+44);
    m->uart_baud=Bu_Read32(p+48); m->minimum_boot_version=Bu_Read32(p+80);
    if (Bu_Read32(p+96)!=IP_MAGIC || p[100]!=IP_VERSION || p[101]!=IP_SIZE ||
        p[102]!=IP_ROLE_BRIDGE_APP || p[103]!=IP_UART ||
        !Bu_Read32(p+104) || Bu_Read32(p+104)==0xFFFFFFFFUL || Bu_Read32(p+108)!=m->minimum_boot_version ||
        Bu_Read32(p+112) || Bu_Read32(p+116) || Bu_Read32(p+120) || Bu_Read32(p+124)) return 0U;
    m->image_role=p[102]; m->update_transports=p[103]; m->irq_abi=Bu_Read32(p+104);
    return m->board_id && m->board_id!=0xFFFFU && m->uart_baud && m->version!=0xFFFFFFFFUL &&
        m->minimum_boot_version!=0xFFFFFFFFUL &&
        m->image_size>=m->minimum_size && m->image_size<=m->capacity &&
        !(m->image_size%4UL) && text_field(p+52,12U,m->family) &&
        text_field(p+64,16U,m->part) && geometry(m->app_start,m->capacity,
            m->flash_size,m->boot_size,m->erase_size,m->minimum_size);
}
uint8_t Bu_ParseInfo(const uint8_t *p, bu_info_t *d) {
    if (Bu_Read32(p)!=BU_INFO_MAGIC || p[4]!=BU_PROTOCOL ||
        (p[5]!=BU_MODE_APP && p[5]!=BU_MODE_BOOT) || !p[6] || p[6]>BU_MAX_DATA ||
        p[6]%4U || (p[7]&0xF8U) || Bu_Read16(p+10)!=2U || !Bu_Read32(p+60) ||
        Bu_Read32(p+60)==0xFFFFFFFFUL || Bu_Read32(p+92)) return 0U;
    memset(d,0,sizeof(*d)); d->mode=p[5]; d->max_data=p[6]; d->flags=p[7];
    d->board_id=Bu_Read16(p+8); d->boot_version=Bu_Read32(p+12); d->boot_build=Bu_Read32(p+16);
    d->app_version=Bu_Read32(p+20); d->app_build=Bu_Read32(p+24); d->installed_size=Bu_Read32(p+28);
    d->app_start=Bu_Read32(p+32); d->capacity=Bu_Read32(p+36); d->flash_size=Bu_Read32(p+40);
    d->boot_size=Bu_Read32(p+44); d->erase_size=Bu_Read32(p+48); d->minimum_size=Bu_Read32(p+52);
    d->uart_baud=Bu_Read32(p+56);
    d->irq_abi=Bu_Read32(p+60);
    if ((d->flags&BU_FLAG_VALID_APP) && (d->installed_size<d->minimum_size ||
        d->installed_size>d->capacity || d->installed_size%4UL || d->app_version==0xFFFFFFFFUL)) return 0U;
    return d->board_id && d->board_id!=0xFFFFU && d->uart_baud &&
        text_field(p+64,16U,d->part) && text_field(p+80,12U,d->family) &&
        geometry(d->app_start,d->capacity,d->flash_size,d->boot_size,d->erase_size,d->minimum_size);
}
void Bu_EncodeInfo(const bu_info_t *d, uint8_t *p) {
    memset(p,0,BU_INFO_SIZE); Bu_Write32(p,BU_INFO_MAGIC); p[4]=BU_PROTOCOL;
    p[5]=d->mode; p[6]=d->max_data; p[7]=d->flags; Bu_Write16(p+8,d->board_id); Bu_Write16(p+10,2U);
    Bu_Write32(p+12,d->boot_version); Bu_Write32(p+16,d->boot_build);
    Bu_Write32(p+20,d->app_version); Bu_Write32(p+24,d->app_build); Bu_Write32(p+28,d->installed_size);
    Bu_Write32(p+32,d->app_start); Bu_Write32(p+36,d->capacity); Bu_Write32(p+40,d->flash_size);
    Bu_Write32(p+44,d->boot_size); Bu_Write32(p+48,d->erase_size); Bu_Write32(p+52,d->minimum_size);
    Bu_Write32(p+56,d->uart_baud); Bu_Write32(p+60,d->irq_abi);
    memcpy(p+64,d->part,16U); memcpy(p+80,d->family,12U);
}
uint8_t Bu_MatchesDevice(const bu_manifest_t *m, const bu_info_t *d) {
    return m->board_id==d->board_id && m->app_start==d->app_start && m->capacity==d->capacity &&
        m->flash_size==d->flash_size && m->boot_size==d->boot_size && m->erase_size==d->erase_size &&
        m->minimum_size==d->minimum_size && m->uart_baud==d->uart_baud &&
        m->image_role==IP_ROLE_BRIDGE_APP && m->update_transports==IP_UART && m->irq_abi==d->irq_abi &&
        m->minimum_boot_version<=d->boot_version && !strcmp(m->family,d->family) && !strcmp(m->part,d->part);
}
uint8_t Bu_ParseMetadataRegion(const uint8_t *p, bu_manifest_t *m) {
    uint16_t i;
    if (!Bu_ParseManifest(p,m)) return 0U;
    for (i=BU_MANIFEST_SIZE; i<BU_METADATA_REGION_SIZE; ++i)
        if (p[i]!=0xFFU) return 0U;
    return 1U;
}
