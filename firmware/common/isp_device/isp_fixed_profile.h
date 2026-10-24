#ifndef ISP_FIXED_PROFILE_H
#define ISP_FIXED_PROFILE_H

/* The native model uses injected geometry/callbacks. An MCU build binds those
 * same operations to its compile-time, independently validated image profile.
 * This avoids repeated far-pointer loads on the 16-bit CPU; no wire field may
 * replace these partition limits. Keep both paths in the native test matrix.
 */
#if defined(__CCRL__) || defined(ISP_TEST_FIXED_PROFILE)
#include "image_metadata_config.h"
#define ISP_CFG(name) ISP_FIXED_##name
#define ISP_FIXED_app_start IMAGE_APPLICATION_START
#define ISP_FIXED_capacity IMAGE_APPLICATION_CAPACITY
#define ISP_FIXED_flash_size IMAGE_CODE_FLASH_SIZE
#define ISP_FIXED_boot_size IMAGE_RESERVED_SIZE
#define ISP_FIXED_erase_size IMAGE_ERASE_BLOCK_SIZE
unsigned char IspMetadataCheck(const unsigned long *words);
unsigned char IspFlashErase(unsigned long address);
unsigned char IspFlashWrite(unsigned long address, const unsigned char *data);
unsigned char IspRomRead(unsigned long address, unsigned char *data, unsigned short size);
unsigned char IspImageVerify(unsigned long size, unsigned long crc);
#define ISP_METADATA(words) IspMetadataCheck(words)
#define ISP_ERASE(address) IspFlashErase(address)
#define ISP_WRITE(address,data) IspFlashWrite(address,data)
#define ISP_READ(address,data,size) IspRomRead(address,data,size)
#define ISP_VERIFY(size,crc) IspImageVerify(size,crc)
#define ISP_CALLBACKS_VALID 1
#define ISP_CAN_READ 1
#else
#define ISP_CFG(name) (device->config->name)
#define ISP_METADATA(words) device->ops->metadata(words)
#define ISP_ERASE(address) device->ops->erase(address)
#define ISP_WRITE(address,data) device->ops->write(address,data)
#define ISP_READ(address,data,size) device->ops->read(address,data,size)
#define ISP_VERIFY(size,crc) device->ops->verify(size,crc)
#define ISP_CALLBACKS_VALID (device->ops && device->ops->metadata && \
    device->ops->erase && device->ops->write && device->ops->read && device->ops->verify)
#define ISP_CAN_READ (device->ops && device->ops->read && device->ops->verify)
#endif

#endif
