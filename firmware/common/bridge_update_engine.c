#include "bridge_update_engine.h"
#include <string.h>

static void invalidate(bu_engine_t *e) {
    e->info.flags &= (uint8_t)(0xFFU ^ BU_FLAG_VALID_APP);
    e->info.app_version=0xFFFFFFFFUL; e->info.app_build=0xFFFFFFFFUL;
    e->info.installed_size=0xFFFFFFFFUL;
}
static uint8_t fail_flash(bu_engine_t *e) {
    invalidate(e); e->info.flags &= (uint8_t)(0xFFU ^ (BU_FLAG_ACTIVE|BU_FLAG_FLASH_READY));
    return BU_FLASH_ERROR;
}
static uint8_t read_image(bu_engine_t *e, uint32_t offset, uint8_t substitute) {
    if (substitute && offset>=BU_MANIFEST_OFFSET && offset<BU_MANIFEST_OFFSET+4UL)
        return e->manifest[(uint16_t)(offset-BU_MANIFEST_OFFSET)];
    return e->flash.read(e->flash.context,e->info.app_start+offset);
}
static uint32_t checksum(bu_engine_t *e, uint32_t length, uint8_t substitute) {
    uint32_t crc, i;
    uint8_t bit;
    crc=0xFFFFFFFFUL;
    for (i=0UL; i<length; ++i) {
        crc ^= read_image(e,i,substitute);
        for (bit=0U; bit<8U; ++bit)
            crc=(crc>>1)^((crc&1UL) ? 0xEDB88320UL : 0UL);
    }
    return crc^0xFFFFFFFFUL;
}
static uint32_t stored_crc(bu_engine_t *e, uint32_t size) {
    uint8_t bytes[4], i;
    for (i=0U; i<4U; ++i) bytes[i]=read_image(e,size-4UL+i,0U);
    return Bu_Read32(bytes);
}
uint8_t Bu_EngineInit(bu_engine_t *e, const bu_info_t *info, const bu_flash_t *flash) {
    uint8_t wire[BU_INFO_SIZE];
    memset(e,0,sizeof(*e));
    Bu_EncodeInfo(info,wire);
    if (!flash || !flash->read || !Bu_ParseInfo(wire,&e->info)) return 0U;
    e->flash=*flash; e->info.flags &= (uint8_t)(0xFFU ^ BU_FLAG_ACTIVE);
    if (!flash->erase || !flash->write4) e->info.flags &= (uint8_t)(0xFFU ^ BU_FLAG_FLASH_READY);
    invalidate(e); return 1U;
}
uint8_t Bu_ValidateInstalled(bu_engine_t *e) {
    uint8_t wire[BU_METADATA_REGION_SIZE];
    uint16_t i;
    bu_manifest_t image;
    if (e->info.flags&BU_FLAG_ACTIVE) return 0U;
    invalidate(e);
    for (i=0U; i<BU_METADATA_REGION_SIZE; ++i)
        wire[i]=read_image(e,BU_MANIFEST_OFFSET+i,0U);
    if (!Bu_ParseMetadataRegion(wire,&image) || !Bu_MatchesDevice(&image,&e->info) ||
        checksum(e,image.image_size-4UL,0U)!=stored_crc(e,image.image_size)) return 0U;
    e->info.app_version=image.version; e->info.app_build=image.build;
    e->info.installed_size=image.image_size; e->info.flags|=BU_FLAG_VALID_APP;
    return 1U;
}
uint8_t Bu_Begin(bu_engine_t *e, const uint8_t *data, uint16_t length) {
    bu_manifest_t image;
    uint32_t crc;
    uint16_t i;
    uint8_t bit;
    if (length!=6U || !data || Bu_Read16(data)!=BU_METADATA_REGION_SIZE) return BU_BAD_ARGUMENT;
    if (e->info.mode!=BU_MODE_BOOT || !(e->info.flags&BU_FLAG_FLASH_READY)) return BU_BAD_STATE;
    if (e->staged!=BU_METADATA_REGION_SIZE) return BU_SEQUENCE_ERROR;
    crc=0xFFFFFFFFUL;
    for (i=0U; i<BU_METADATA_REGION_SIZE; ++i) {
        crc^=e->manifest[i];
        for (bit=0U; bit<8U; ++bit) crc=(crc>>1)^((crc&1UL)?0xEDB88320UL:0UL);
    }
    if ((crc^0xFFFFFFFFUL)!=Bu_Read32(data+2U)) return BU_CRC_ERROR;
    if (!Bu_ParseMetadataRegion(e->manifest,&image) || !Bu_MatchesDevice(&image,&e->info)) return BU_INCOMPATIBLE;
    if (e->info.flags&BU_FLAG_ACTIVE) return BU_OK;
    e->image=image; e->next=0UL;
    /* Invalidate before the first erase, including an uncertain erase failure. */
    invalidate(e);
    if (!e->flash.erase(e->flash.context,e->info.app_start)) return fail_flash(e);
    e->erased_block=e->info.app_start; e->info.flags|=BU_FLAG_ACTIVE;
    return BU_OK;
}
uint8_t Bu_Data(bu_engine_t *e, const uint8_t *data, uint16_t length) {
    uint32_t offset, current, address, block;
    uint16_t count, i;
    if (!data || length<=4U || length>4U+e->info.max_data || (length-4U)%4U) return BU_BAD_ARGUMENT;
    if (!(e->info.flags&BU_FLAG_ACTIVE)) return BU_BAD_STATE;
    offset=Bu_Read32(data); count=(uint16_t)(length-4U);
    if (offset%4UL || offset>e->image.image_size || count>e->image.image_size-offset) return BU_BAD_ARGUMENT;
    if (offset<e->next) {
        if (count>e->next-offset) return BU_SEQUENCE_ERROR;
        for (i=0U; i<count; ++i)
            if (read_image(e,offset+i,1U)!=data[i+4U]) return BU_SEQUENCE_ERROR;
        return BU_OK;
    }
    if (offset!=e->next) return BU_SEQUENCE_ERROR;
    /* The streamed manifest must equal BEGIN before any part of this chunk is written. */
    for (i=0U; i<count; ++i) {
        current=offset+i;
        if (current>=BU_MANIFEST_OFFSET && current<BU_MANIFEST_OFFSET+BU_METADATA_REGION_SIZE &&
            data[i+4U]!=e->manifest[(uint16_t)(current-BU_MANIFEST_OFFSET)]) return BU_INCOMPATIBLE;
    }
    for (i=0U; i<count; i=(uint16_t)(i+4U)) {
        current=offset+i; address=e->info.app_start+current;
        block=address-address%e->info.erase_size;
        if (block!=e->erased_block) {
            if (!e->flash.erase(e->flash.context,block)) return fail_flash(e);
            e->erased_block=block;
        }
        /* Commit marker is written only after full image CRC validation. */
        if (current==BU_MANIFEST_OFFSET) continue;
        if (!e->flash.write4(e->flash.context,address,data+i+4U)) return fail_flash(e);
        if (read_image(e,current,0U)!=data[i+4U] || read_image(e,current+1UL,0U)!=data[i+5U] ||
            read_image(e,current+2UL,0U)!=data[i+6U] || read_image(e,current+3UL,0U)!=data[i+7U]) return fail_flash(e);
    }
    e->next+=count; return BU_OK;
}
uint8_t Bu_End(bu_engine_t *e, const uint8_t *data, uint16_t length) {
    uint32_t expected;
    if (!data || length!=4U) return BU_BAD_ARGUMENT;
    if (!(e->info.flags&BU_FLAG_ACTIVE) || e->next!=e->image.image_size) return BU_BAD_STATE;
    expected=Bu_Read32(data);
    if (stored_crc(e,e->image.image_size)!=expected || checksum(e,e->image.image_size-4UL,1U)!=expected) {
        Bu_Abort(e); return BU_CRC_ERROR;
    }
    if (!e->flash.write4(e->flash.context,e->info.app_start+BU_MANIFEST_OFFSET,e->manifest)) return fail_flash(e);
    e->info.flags &= (uint8_t)(0xFFU ^ BU_FLAG_ACTIVE);
    return Bu_ValidateInstalled(e) ? BU_OK : fail_flash(e);
}
void Bu_Abort(bu_engine_t *e) {
    e->info.flags &= (uint8_t)(0xFFU ^ BU_FLAG_ACTIVE); e->staged=0U;
}
uint8_t Bu_StageMetadata(bu_engine_t *e, const uint8_t *p, uint16_t length) {
    uint16_t offset, count;
    if (!p || length<=4U || length>4U+BU_METADATA_CHUNK_SIZE) return BU_BAD_ARGUMENT;
    if (e->info.mode!=BU_MODE_BOOT || !(e->info.flags&BU_FLAG_FLASH_READY) ||
        (e->info.flags&BU_FLAG_ACTIVE)) return BU_BAD_STATE;
    offset=Bu_Read16(p); count=(uint16_t)(length-4U);
    if (Bu_Read16(p+2U)!=BU_METADATA_REGION_SIZE || offset>=BU_METADATA_REGION_SIZE ||
        count>BU_METADATA_REGION_SIZE-offset) return BU_BAD_ARGUMENT;
    if (offset<e->staged) {
        if (count>e->staged-offset || memcmp(p+4U,e->manifest+offset,count)) return BU_SEQUENCE_ERROR;
        return BU_OK;
    }
    if (offset!=e->staged) return BU_SEQUENCE_ERROR;
    memcpy(e->manifest+offset,p+4U,count); e->staged=(uint16_t)(offset+count);
    return BU_OK;
}
uint8_t Bu_ReadMetadata(bu_engine_t *e, const uint8_t *p, uint16_t length,
                        uint8_t *reply, uint16_t *reply_length) {
    uint16_t offset, count, i;
    *reply_length=0U;
    if (!p || length!=4U || (e->info.flags&BU_FLAG_ACTIVE)) return BU_BAD_ARGUMENT;
    offset=Bu_Read16(p); count=Bu_Read16(p+2U);
    if (!count || count>BU_METADATA_CHUNK_SIZE || offset>=BU_METADATA_REGION_SIZE ||
        count>BU_METADATA_REGION_SIZE-offset) return BU_BAD_ARGUMENT;
    Bu_Write16(reply,offset); Bu_Write16(reply+2U,BU_METADATA_REGION_SIZE);
    for (i=0U; i<count; ++i) reply[4U+i]=read_image(e,BU_MANIFEST_OFFSET+offset+i,0U);
    *reply_length=(uint16_t)(4U+count); return BU_OK;
}
