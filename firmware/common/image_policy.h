#ifndef IMAGE_POLICY_H
#define IMAGE_POLICY_H

/* Shared image policy at metadata offset96. Independent of runtime peripherals
 * and of the PC-to-bridge Host Link transport. All words are little-endian. */
#define IP_OFFSET (96U)
#define IP_SIZE (32U)
#define IP_CORE_SIZE (128U)
#define IP_MAGIC (0x31445055UL) /* "UPD1" */
#define IP_VERSION (1U)
/* Version2 is a DEVICE UART profile. Existing CAN/HOST version1 bytes stay
 * unchanged. A future UART target packager must emit its compile-time baud. */
#define IP_DEVICE_UART_VERSION (2U)
#define IP_DEVICE_UART_BAUD_OFFSET (112U)
#define IP_ROLE_DEVICE_APP (1U)
#define IP_ROLE_BRIDGE_APP (2U)
#define IP_CAN (1U)
#define IP_I2C (2U)
#define IP_UART (4U)
#define IP_ALL (7U)
#define IP_F13_IRQ_ABI (0x31563146UL) /* "F1V1" */
#define IP_F24_IRQ_ABI (0x31563246UL) /* "F2V1" */
#endif
