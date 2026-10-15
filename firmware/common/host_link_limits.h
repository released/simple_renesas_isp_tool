#ifndef HOST_LINK_LIMITS_H
#define HOST_LINK_LIMITS_H
/* Shared wire limits, independent of metadata region and bus data sizes.
 * UART rings have255 usable bytes; a complete142-byte frame fits in one enqueue. */
#define HOST_WIRE_MAX_PAYLOAD (128U)
#define HOST_WIRE_HEADER_SIZE (12U)
#define HOST_WIRE_CRC_SIZE (2U)
#define HOST_WIRE_MAX_FRAME (HOST_WIRE_HEADER_SIZE + HOST_WIRE_MAX_PAYLOAD + HOST_WIRE_CRC_SIZE)
#endif
