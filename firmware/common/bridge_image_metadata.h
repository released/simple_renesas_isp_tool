#ifndef BRIDGE_IMAGE_METADATA_H
#define BRIDGE_IMAGE_METADATA_H
#include <stdint.h>
#include <limits.h>
#include "image_policy.h"
#if CHAR_BIT != 8
#error Bridge metadata requires 8-bit bytes.
#endif
/* Portable image/descriptor contract. No command IDs, Flash backend or UART.
 * Wire integers are little-endian; use byte readers, not packed C structs.
 * The core, Flash region and live descriptor have independent lengths.
 */
typedef char BuWordMustBe32Bits[(sizeof(uint32_t) == 4U) ? 1 : -1];
#ifdef __cplusplus
extern "C" {
#endif
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
uint8_t Bu_ParseManifest(const uint8_t *p, uint16_t length, bu_manifest_t *out);
uint8_t Bu_ParseMetadataRegion(const uint8_t *p, uint16_t length, bu_manifest_t *out);
uint8_t Bu_ParseInfo(const uint8_t *p, uint16_t length, bu_info_t *out);
void Bu_EncodeInfo(const bu_info_t *in, uint8_t *p);
uint8_t Bu_MatchesDevice(const bu_manifest_t *image, const bu_info_t *device);
/* Parse calls require a non-null output and sufficient input length.
 * Return1 only for complete format validation, not CRC/authentication/launch.
 * Outputs must be ignored on failure. Buffer bytes are never changed.
 * Core parsing alone does not check the reserved half of the Flash region.
 * Bu_Read/Write helpers require a caller-validated2/4-byte window.
 */
#ifdef __cplusplus
}
#endif
#endif

