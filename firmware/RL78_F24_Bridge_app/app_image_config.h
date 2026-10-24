#ifndef APP_IMAGE_CONFIG_H
#define APP_IMAGE_CONFIG_H

/* App release version: packed 0x00MMmmpp, one byte per component.
 * Example0x00010600UL is1.6.0. This identifies the App; it does not enforce
 * a downgrade policy. Boot and App release versions need not be equal.
 */
#define APP_IMAGE_VERSION (0x00010801UL)

/* App build identification. YYYYMMDD is the chosen convention, not a clock.
 * Example20261017UL. Values0..0xFFFFFFFE are allowed.
 */
#define APP_IMAGE_BUILD (20261023UL)

/* Local host board identity. Match F24_BRIDGE_BOARD_ID in the shared image
 * metadata config. Changing it alone fails pre-build, not MCU selection.
 * Example0x0101U selects this board contract;0xF33BU does not match it.
 */
#define APP_IMAGE_TARGET_ID (0x0101U)

/* Image destination role:2=HOST BRIDGE APP,1=DEVICE APP.
 * This project requires2. A value such as5U fails generation.
 */
#define APP_IMAGE_ROLE (2U)

/* Allowed App update transport bitmap:0x01=CAN,0x02=I2C,0x04=UART.
 * This Host App is installed through Misc/UART and requires0x04U.
 * Its normal CAN/I2C bridge functions are unrelated to this permission.
 * UART115200, Flash geometry, IRQ ABI and minimum Boot are engineering
 * settings in image_metadata_config.h, not App release speed selections.
 */
#define APP_IMAGE_UPDATE_TRANSPORTS (0x04U)
#endif
