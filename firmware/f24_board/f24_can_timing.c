#include "platform.h"
#include "f24_can_clock.h"
#include "f24_can_timing.h"

uint8_t F24_CanTimingGet(uint8_t preset, f24_can_nominal_timing_t *timing)
{
    uint16_t brp;
    uint16_t tseg1;
    uint16_t tseg2;
    uint16_t sjw;
    uint32_t bitrate;

    if (timing == (f24_can_nominal_timing_t *)0)
    {
        return 0U;
    }
    brp = 0U;
    tseg1 = 63U;
    tseg2 = 16U;
    sjw = 16U;
    switch (preset)
    {
    case F24_CAN_NOMINAL_250K:
        brp = 1U;
        bitrate = 250000UL;
        break;
    case F24_CAN_NOMINAL_500K:
        bitrate = 500000UL;
        break;
    case F24_CAN_NOMINAL_1M:
        tseg1 = 31U;
        tseg2 = 8U;
        sjw = 8U;
        bitrate = 1000000UL;
        break;
    default:
        return 0U;
    }
    /* All presets sample at 80%. 40 MHz / (BRP+1) / (1+TSEG1+TSEG2).
     * The SJW high bit is bit 0 of C0NCFGH; low six bits are C0NCFGL[15:10]. */
    timing->high = (uint16_t)(((tseg2 - 1U) << 9) | ((tseg1 - 1U) << 1) | ((sjw - 1U) >> 6));
    timing->low = (uint16_t)((((sjw - 1U) & 0x3FU) << 10) | brp);
    timing->bitrate = bitrate;
    return 1U;
}

uint8_t F24_CanTimingApply(uint8_t preset)
{
    f24_can_nominal_timing_t timing;

    if ((F24_CanTimingGet(preset, &timing) == 0U) || (F24_CanClockIsReady() == 0U) ||
        ((GSTS & 0x07U) != 0x01U) || ((C0STSL & 0x07U) != 0x01U))
    {
        return 0U;
    }
    C0NCFGH = timing.high;
    C0NCFGL = timing.low;
    return 1U;
}
