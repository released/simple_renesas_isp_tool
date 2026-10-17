#ifndef BOOT_APP_HANDOFF_H
#define BOOT_APP_HANDOFF_H

/* Shared no-init RAM outside startup-cleared and linker-allocated sections.
 * The recovery cookie asks Boot to stay after reset. IRQ dispatch ownership
 * is independent of image metadata and is set only by the Boot/App handoff.
 * Reserve four bytes at each address, including the byte-sized IRQ selector.
 * Changing these addresses requires matching startup/linker/dispatch changes.
 */
#define F24_BRIDGE_BOOT_COOKIE (0xB00724A5UL)
#define F24_BRIDGE_COOKIE_ADDRESS (0x000FF700UL)
#define F24_BRIDGE_DISPATCH_ADDRESS (0x000FF710UL)
/* Current dispatch encoding; only APP_OWNER routes normal IRQs to App slots. */
#define F24_BRIDGE_BOOT_OWNER (0x00U)
#define F24_BRIDGE_APP_OWNER (0xA5U)
#endif
