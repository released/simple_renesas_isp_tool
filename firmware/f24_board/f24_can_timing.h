#ifndef F24_CAN_TIMING_H
#define F24_CAN_TIMING_H
#include <stdint.h>

/* Host Link v2 preset numbers, not dropdown indexes. Nominal preset 3
 * denotes 800K and is not implemented by this bridge. */
#define F24_CAN_NOMINAL_250K (1U)
#define F24_CAN_NOMINAL_500K (2U)
#define F24_CAN_NOMINAL_1M (4U)
#define F24_CAN_NOMINAL_MASK ((1UL << 1) | (1UL << 2) | (1UL << 4))

typedef struct
{
    uint16_t high;
    uint16_t low;
    uint32_t bitrate;
} f24_can_nominal_timing_t;

uint8_t F24_CanTimingGet(uint8_t preset, f24_can_nominal_timing_t *timing);
/* Only writes in global/channel reset mode with the validated 40-MHz clock.
 * SET_CAN_CONFIG applies it while stopped; CAN_START reapplies it after
 * R_CAN_SetConfig, which otherwise restores the generated 500K timing. */
uint8_t F24_CanTimingApply(uint8_t preset);
#endif
