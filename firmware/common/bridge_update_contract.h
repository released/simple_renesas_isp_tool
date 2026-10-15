#ifndef BRIDGE_UPDATE_CONTRACT_H
#define BRIDGE_UPDATE_CONTRACT_H
#include <stdint.h>
#include "image_policy.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Bridge self-update is a separate destination/contract, never a CAN target
 * profile. The host learns geometry from this descriptor, not an MCU name.
 * All wire fields are little-endian. Code-flash writes may touch app only. */
#define BU_FEATURE (0x00000400UL)
#define BU_INFO_COMMAND (0x50U)
#define BU_ENTER_COMMAND (0x51U)
#define BU_BEGIN_COMMAND (0x52U)
#define BU_DATA_COMMAND (0x53U)
#define BU_END_COMMAND (0x54U)
#define BU_ABORT_COMMAND (0x55U)
#define BU_RUN_COMMAND (0x56U)
#define BU_READ_METADATA_COMMAND (0x57U)
#define BU_STAGE_METADATA_COMMAND (0x58U)
#define BU_PROTOCOL (3U)
/* Core/descriptor sizes are NOT metadata region size or UART payload size. */
#define BU_MANIFEST_SIZE (IP_CORE_SIZE)
#define BU_INFO_SIZE (96U)
#define BU_METADATA_REGION_SIZE (256U)
#define BU_METADATA_CHUNK_SIZE (64U)
#define BU_MANIFEST_OFFSET (0x100UL)
#define BU_IMAGE_MAGIC (0x31525542UL)
#define BU_INFO_MAGIC (0x31495542UL)
#define BU_MODE_APP (1U)
#define BU_MODE_BOOT (2U)
#define BU_FLAG_VALID_APP (1U)
#define BU_FLAG_ACTIVE (2U)
#define BU_FLAG_FLASH_READY (4U)
#define BU_MAX_DATA (64U)
#define BU_OK (0U)
#define BU_BAD_ARGUMENT (3U)
#define BU_CRC_ERROR (4U)
#define BU_SEQUENCE_ERROR (5U)
#define BU_INCOMPATIBLE (9U)
#define BU_BAD_STATE (10U)
#define BU_FLASH_ERROR (11U)

typedef struct {
    uint16_t board_id;
    uint32_t image_size, version, build;
    uint32_t app_start, capacity, flash_size, boot_size, erase_size, minimum_size;
    uint32_t uart_baud, minimum_boot_version;
    uint8_t image_role, update_transports;
    uint32_t irq_abi;
    char family[13], part[17];
} bu_manifest_t;
typedef struct {
    uint8_t mode, max_data, flags;
    uint16_t board_id;
    uint32_t boot_version, boot_build, app_version, app_build, installed_size;
    uint32_t app_start, capacity, flash_size, boot_size, erase_size, minimum_size;
    uint32_t uart_baud;
    uint32_t irq_abi;
    char family[13], part[17];
} bu_info_t;
uint16_t Bu_Read16(const uint8_t *p);
uint32_t Bu_Read32(const uint8_t *p);
void Bu_Write16(uint8_t *p, uint16_t n);
void Bu_Write32(uint8_t *p, uint32_t n);
uint8_t Bu_ParseManifest(const uint8_t *p, bu_manifest_t *out);
uint8_t Bu_ParseMetadataRegion(const uint8_t *p, bu_manifest_t *out);
uint8_t Bu_ParseInfo(const uint8_t *p, bu_info_t *out);
void Bu_EncodeInfo(const bu_info_t *in, uint8_t *p);
uint8_t Bu_MatchesDevice(const bu_manifest_t *image, const bu_info_t *device);
#ifdef __cplusplus
}
#endif
#endif
