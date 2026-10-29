#ifndef BRIDGE_UPDATE_CONTRACT_H
#define BRIDGE_UPDATE_CONTRACT_H
#include <stdint.h>
#include "bridge_image_metadata.h"
#ifdef __cplusplus
extern "C"
{
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
#define BU_DIAGNOSTICS_COMMAND (0x59U)
#define BU_HOLD_NONE (0U)
#define BU_HOLD_NO_APP (1U)
#define BU_HOLD_METADATA (2U)
#define BU_HOLD_CONTRACT (3U)
#define BU_HOLD_CRC (4U)
#define BU_HOLD_RECOVERY (5U)
#define BU_HOLD_FLASH (6U)
#define BU_HOLD_ENTER (7U)
#define BU_HOLD_UPDATED (8U)
#define BU_OK (0U)
#define BU_BAD_ARGUMENT (3U)
#define BU_CRC_ERROR (4U)
#define BU_SEQUENCE_ERROR (5U)
#define BU_INCOMPATIBLE (9U)
#define BU_BAD_STATE (10U)
#define BU_FLASH_ERROR (11U)

#ifdef __cplusplus
}
#endif
#endif
