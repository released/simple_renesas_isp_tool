#ifndef HOST_LINK_BAUD_H
#define HOST_LINK_BAUD_H
/* PC-to-bridge link only. Never a device UART update setting. */
#define HL_BAUD_FEATURE (0x00000800UL)
#define HL_BAUD_INFO (0x05U)
#define HL_BAUD_SET (0x06U)
#define HL_BAUD_CONFIRM (0x07U)
#define HL_BAUD_DEFAULT (115200UL)
#define HL_BAUD_MASK (7UL)
#define HL_BAUD_CONFIRM_MS (2000U)
#define HL_BAUD_INFO_SIZE (20U)
#endif
