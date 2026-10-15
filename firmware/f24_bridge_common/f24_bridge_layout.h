#ifndef F24_BRIDGE_LAYOUT_H
#define F24_BRIDGE_LAYOUT_H
/* Single geometry source for linker generation, BIN packaging and MCU guards.
 * Actual board: R7F124FLJ3xFB, 256 KiB code flash, 20 MHz X1.
 * Boot UART uses HOCO40; app performs the existing bounded X1/PLL startup.
 * Last 1 KiB includes the CS+ debug monitor and is never erased by boot.
 * Changing this layout requires reinstalling a matching boot, not just an app. */
#define F24_BRIDGE_APP_START (0x00005000UL)
#define F24_BRIDGE_APP_LIMIT (0x0003FC00UL)
#define F24_BRIDGE_FLASH_SIZE (0x00040000UL)
#define F24_BRIDGE_ERASE_SIZE (1024UL)
#define F24_BRIDGE_MINIMUM_SIZE (516UL)
#define F24_BRIDGE_UART_BAUD (115200UL)
#define F24_BRIDGE_BOARD_ID (0x0101U)
#define F24_BRIDGE_BOOT_VERSION (0x00010200UL)
/* Boot build only. App version/build live in each app's app_image.json. */
#define F24_BRIDGE_BUILD (20261015UL)
#define F24_BRIDGE_BOOT_COOKIE (0xB00724A5UL)
/* FF700..FF710 is reserved outside all startup-cleared / allocated sections. */
#define F24_BRIDGE_COOKIE_ADDRESS (0x000FF700UL)
#define F24_BRIDGE_DISPATCH_ADDRESS (0x000FF710UL)
#endif
