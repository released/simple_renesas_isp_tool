#ifndef CAN_BURST_H
#define CAN_BURST_H
#include <stdint.h>
#define CB_COMMAND (0x22U)
#define CB_FEATURE (0x00001000UL)
#define CB_VERSION (1U)
#define CB_HEADER (12U)
#define CB_MAX_FRAMES (10U)
/* Compact standard Classical data records: LE16 ID, DLC, DLC data bytes. */
typedef struct
{
    uint8_t payload[128];
    uint8_t cursor;
    uint8_t remaining;
} cb_cursor_t;
#endif
