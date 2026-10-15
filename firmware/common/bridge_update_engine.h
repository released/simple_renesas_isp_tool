#ifndef BRIDGE_UPDATE_ENGINE_H
#define BRIDGE_UPDATE_ENGINE_H
#include "bridge_update_contract.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void *context;
    uint8_t (*read)(void *context, uint32_t address);
    uint8_t (*erase)(void *context, uint32_t address);
    uint8_t (*write4)(void *context, uint32_t address, const uint8_t *data);
} bu_flash_t;
typedef struct {
    bu_info_t info;
    bu_flash_t flash;
    bu_manifest_t image;
    uint8_t manifest[BU_METADATA_REGION_SIZE];
    uint16_t staged;
    uint32_t next, erased_block;
} bu_engine_t;
uint8_t Bu_EngineInit(bu_engine_t *engine, const bu_info_t *info, const bu_flash_t *flash);
uint8_t Bu_ValidateInstalled(bu_engine_t *engine);
/* Return Host Link status. Length is checked before dereferencing a payload. */
uint8_t Bu_Begin(bu_engine_t *engine, const uint8_t *data, uint16_t length);
uint8_t Bu_StageMetadata(bu_engine_t *engine, const uint8_t *data, uint16_t length);
uint8_t Bu_ReadMetadata(bu_engine_t *engine, const uint8_t *data, uint16_t length,
                        uint8_t *reply, uint16_t *reply_length);
uint8_t Bu_Data(bu_engine_t *engine, const uint8_t *data, uint16_t length);
uint8_t Bu_End(bu_engine_t *engine, const uint8_t *data, uint16_t length);
void Bu_Abort(bu_engine_t *engine);
#ifdef __cplusplus
}
#endif
#endif
