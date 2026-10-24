#ifndef ISP_SMALL_USER_DEF_H
#define ISP_SMALL_USER_DEF_H
#include "image_metadata_config.h"
#include "isp_transport_config.h"
#include "boot_app_handoff.h"
#define FSL_OK (0U)
#define APP_FLASH_START IMAGE_APPLICATION_START
#define FLASH_UPDATE_MAX_SIZE IMAGE_APPLICATION_CAPACITY
#define FLASH_BYTE_PER_BLOCK IMAGE_ERASE_BLOCK_SIZE
#endif
