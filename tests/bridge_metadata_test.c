/* Strict C90 portability/extent tests. No UART, update engine or Flash API. */
#include "bridge_image_metadata.h"
#include "image_metadata_profile.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    uint8_t bytes[BU_METADATA_REGION_SIZE + 1U];
    uint8_t before[sizeof(bytes)], wire[BU_INFO_SIZE];
    uint8_t *p;
    uint16_t i;
    bu_info_t d, decoded;
    bu_manifest_t m;
    F24Bridge_Info(&d,BU_MODE_BOOT);
    Bu_EncodeInfo(&d,wire);
    assert(Bu_ParseInfo(wire,sizeof(wire),&decoded));
    for (i=0U; i<BU_INFO_SIZE; ++i) assert(!Bu_ParseInfo(wire,i,&decoded));
    assert(!Bu_ParseInfo(0,BU_INFO_SIZE,&decoded));
    assert(!Bu_ParseInfo(wire,BU_INFO_SIZE,0));
    memset(bytes,0xFF,sizeof(bytes));
    p=bytes+1U; /* Deliberately unaligned wire buffer. */
    memset(p,0,BU_MANIFEST_SIZE);
    Bu_Write32(p,BU_IMAGE_MAGIC); p[4]=BU_PROTOCOL; p[5]=BU_MANIFEST_SIZE;
    p[6]=IP_ROLE_BRIDGE_APP; p[7]=1U;
    Bu_Write16(p+8U,d.board_id); Bu_Write16(p+10U,2U);
    Bu_Write32(p+12U,1024UL); Bu_Write32(p+16U,0x10600UL);
    Bu_Write32(p+20U,20261017UL); Bu_Write32(p+24U,d.app_start);
    Bu_Write32(p+28U,d.capacity); Bu_Write32(p+32U,d.flash_size);
    Bu_Write32(p+36U,d.boot_size); Bu_Write32(p+40U,d.erase_size);
    Bu_Write32(p+44U,d.minimum_size); Bu_Write32(p+48U,d.uart_baud);
    memcpy(p+52U,d.family,12U); memcpy(p+64U,d.part,16U);
    Bu_Write32(p+80U,d.boot_version); Bu_Write32(p+96U,IP_MAGIC);
    p[100]=IP_VERSION; p[101]=IP_SIZE; p[102]=IP_ROLE_BRIDGE_APP; p[103]=IP_UART;
    Bu_Write32(p+104U,d.irq_abi); Bu_Write32(p+108U,d.boot_version);
    memcpy(before,bytes,sizeof(bytes));
    assert(Bu_ParseManifest(p,BU_MANIFEST_SIZE,&m));
    assert(Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m));
    assert(Bu_MatchesDevice(&m,&d));
    assert(!memcmp(before,bytes,sizeof(bytes)));
    for (i=0U; i<BU_MANIFEST_SIZE; ++i) assert(!Bu_ParseManifest(p,i,&m));
    for (i=0U; i<BU_METADATA_REGION_SIZE; ++i) assert(!Bu_ParseMetadataRegion(p,i,&m));
    assert(!Bu_ParseManifest(0,BU_MANIFEST_SIZE,&m));
    assert(!Bu_ParseManifest(p,BU_MANIFEST_SIZE,0));
    assert(!Bu_ParseMetadataRegion(0,BU_METADATA_REGION_SIZE,&m));
    assert(!Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,0));
    for (i=84U; i<BU_METADATA_REGION_SIZE; ++i) {
        if ((i>=96U && i<112U)) continue;
        p[i]^=1U; assert(!Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m));
        p[i]^=1U; assert(Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m));
    }
    p[103]=IP_CAN; assert(!Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m)); p[103]=IP_UART;
    p[6]=IP_ROLE_DEVICE_APP; assert(!Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m)); p[6]=IP_ROLE_BRIDGE_APP;
    p[64]='X'; assert(Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m));
    assert(!Bu_MatchesDevice(&m,&d)); p[64]=(uint8_t)d.part[0];
    Bu_Write32(p+12U,d.capacity+4UL); assert(!Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m));
    Bu_Write32(p+12U,1024UL); assert(Bu_ParseMetadataRegion(p,BU_METADATA_REGION_SIZE,&m));
    assert(Bu_MatchesDevice(&m,&d));
    return 0;
}
