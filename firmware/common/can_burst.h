#ifndef CAN_BURST_H
#define CAN_BURST_H
#include <stdint.h>
#define CB_COMMAND (0x22U)
#define CB_FEATURE (0x00001000UL)
#define CB_FD_FEATURE (0x00002000UL)
#define CB_VERSION (1U)
#define CB_FD_VERSION (2U)
#define CB_FD_FLAGS (0x0CU)
#define CB_HEADER (12U)
#define CB_MAX_FRAMES (10U)
/* Standard records: LE16 ID, wire DLC, decoded-length data bytes.
 * Version1 is Classical; version2 is FD+BRS for every record and the ACK. */
typedef struct
{
    uint8_t payload[128];
    uint8_t cursor;
    uint8_t remaining;
} cb_cursor_t;
#endif
