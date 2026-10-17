#ifndef IMAGE_METADATA_CONFIG_H
#define IMAGE_METADATA_CONFIG_H
/* Single geometry source for linker generation, BIN packaging and MCU guards.
 * Actual board: R7F124FLJ3xFB, 256 KiB code flash, 20 MHz X1.
 * Boot UART uses HOCO40; app performs the existing bounded X1/PLL startup.
 * Last 1 KiB includes the CS+ debug monitor and is never erased by boot.
 * Changing this layout requires reinstalling a matching boot, not just an app. */
/* First App byte and Boot exclusive end. Example0x5000 reserves20KiB.
 * Align the two linker settings and reinstall Boot after a partition change.
 */
#define F24_BRIDGE_APP_START (0x00005000UL)
/* Exclusive App end;0x3FC00 protects the final1KiB of this256KiB device. */
#define F24_BRIDGE_APP_LIMIT (0x0003FC00UL)
/* Physical code Flash bytes; match the selected CS+ device and Flash backend. */
#define F24_BRIDGE_FLASH_SIZE (0x00040000UL)
/* Code Flash erase block bytes, not the Host Link data chunk length. */
#define F24_BRIDGE_ERASE_SIZE (1024UL)
/* 256-byte vectors +256-byte metadata +4-byte CRC tail =516 minimum bytes. */
#define F24_BRIDGE_MINIMUM_SIZE (516UL)
/* Fixed Boot/Misc UART speed. Normal App Host Link negotiation is independent. */
#define F24_BRIDGE_UART_BAUD (115200UL)
/* Logical board identity reported live and required by App metadata. */
#define F24_BRIDGE_BOARD_ID (0x0101U)
/* Boot release0x00MMmmpp. Also the minimum Boot required by this App package. */
#define F24_BRIDGE_BOOT_VERSION (0x00010300UL)
/* Boot build only. App releases edit their own app_image_config.h. */
#define F24_BRIDGE_BUILD (20261017UL)
/* Engineering identity, zero-padded into the BUR1 metadata and live BUI1.
 * Family supports up to12 printable ASCII bytes, part up to16.
 * Match the actual project device, protected Flash backend and Tool descriptor.
 */
#define F24_BRIDGE_FAMILY "RL78/F24"
#define F24_BRIDGE_PART "R7F124FLJ3xFB"
/* Fixed interrupt forwarding ABI, not the number of enabled App interrupts. */
#define F24_BRIDGE_IRQ_ABI (0x31563246UL)
#endif
