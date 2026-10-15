#include <stdio.h>
#include "f24_clock_mock/platform.h"
#include "../firmware/f24_board/f24_can_clock.h"
#include "../firmware/f24_board/f24_can_timing.h"

volatile mock_register_t mock_ckc, mock_cksel, mock_pllctl, mock_pllsts;
volatile uint8_t MDIV, HIOSTOP, MSTOP, OSTS, OSTC;
volatile uint16_t GSTS, C0STSL, C0NCFGH, C0NCFGL;
static unsigned mock_fault;
static uint8_t mock_cmc;

uint8_t MockClockReadCmc(void)
{
    return mock_cmc;
}

void MockClockAdvance(void)
{
    if ((MSTOP == 0U) && ((CMC & 0xC1U) == 0x41U) && (mock_fault != 1U))
    {
        OSTC = 0xFFU;
    }
    MCS = (uint8_t)((MCM0 != 0U) && (mock_fault != 2U));
    if ((PLLON != 0U) && (mock_fault != 3U) &&
        ((CKSEL & 0xE0U) == 0xE0U) && ((PLLCTL & 0xFBU) == 0xE3U))
    {
        mock_pllsts.bits.b7 = 1U;
    }
    SELPLLS = (uint8_t)((SELPLL != 0U) &&
        (mock_pllsts.bits.b7 != 0U) && (mock_fault != 4U));
}

static void ResetMock(unsigned fault)
{
    mock_fault = fault;
    CKC = 0U;
    CKSEL = 0U;
    PLLCTL = 0U;
    PLLSTS = 0U;
    MDIV = 0U;
    HIOSTOP = 0U;
    MSTOP = 1U;
    /* Model BSP's first and locked CMC write, not a writable RAM variable. */
    mock_cmc = 0x51U;
    OSTS = 0U;
    OSTC = 0U;
}

static int TestNominalTiming(void)
{
    static const uint8_t presets[] = {1U, 2U, 4U};
    static const uint32_t rates[] = {250000UL, 500000UL, 1000000UL};
    f24_can_nominal_timing_t timing;
    uint16_t tseg1;
    uint16_t tseg2;
    uint16_t sjw;
    uint16_t brp;
    unsigned i;

    GSTS = 1U;
    C0STSL = 1U;
    for (i = 0U; i < 3U; i++)
    {
        if (F24_CanTimingGet(presets[i], &timing) == 0U) return 1;
        tseg1 = (uint16_t)(((timing.high >> 1) & 0xFFU) + 1U);
        tseg2 = (uint16_t)(((timing.high >> 9) & 0x7FU) + 1U);
        sjw = (uint16_t)((((timing.high & 1U) << 6) |
                        ((timing.low >> 10) & 0x3FU)) + 1U);
        brp = (uint16_t)((timing.low & 0x3FFU) + 1U);
        if ((40000000UL / brp / (1UL + tseg1 + tseg2) != rates[i]) ||
            (timing.bitrate != rates[i]) || (tseg1 <= tseg2) ||
            (sjw > tseg2) || ((1UL + tseg1) * 100UL /
                             (1UL + tseg1 + tseg2) != 80UL) ||
            (F24_CanTimingApply(presets[i]) == 0U) ||
            (C0NCFGH != timing.high) || (C0NCFGL != timing.low)) return 1;
    }
    C0NCFGH = 0xAAAAU;
    C0NCFGL = 0xBBBBU;
    GSTS = 0U;
    if (F24_CanTimingApply(1U) != 0U) return 1;
    GSTS = 1U;
    C0STSL = 0U;
    if (F24_CanTimingApply(4U) != 0U) return 1;
    C0STSL = 1U;
    for (i = 0U; i < 256U; i++)
    {
        if ((i != 1U) && (i != 2U) && (i != 4U) &&
            ((F24_CanTimingGet((uint8_t)i, &timing) != 0U) ||
             (F24_CanTimingApply((uint8_t)i) != 0U))) return 1;
    }
    if ((C0NCFGH != 0xAAAAU) || (C0NCFGL != 0xBBBBU) ||
        (F24_CanTimingGet(1U, (f24_can_nominal_timing_t *)0) != 0U)) return 1;
    return 0;
}

int TestF24CanClock(void)
{
    unsigned fault;
    uint8_t result;
    uint8_t diagnostic[8];
    uint32_t nominal_rate;
    uint32_t data_rate;
    uint8_t nominal_sjw;
    uint8_t data_sjw;
    uint8_t can_pin_route;
    static const uint8_t expected[] = {0U, 2U, 3U, 4U, 5U};

    nominal_rate = 40000000UL / (CAN_CFG_NBRP + 1UL) /
        (1UL + CAN_CFG_NMNL_TSEG1 + CAN_CFG_NMNL_TSEG2);
    data_rate = 40000000UL / (CAN_CFG_DBRP + 1UL) /
        (1UL + CAN_CFG_DATA_TSEG1 + CAN_CFG_DATA_TSEG2);
    nominal_sjw = CAN_CFG_NSJW;
    data_sjw = CAN_CFG_DSJW;
    can_pin_route = BSP_CFG_PIOR46;
    if ((nominal_rate != 500000UL) || (data_rate != 2000000UL) ||
        (nominal_sjw != 15U) || (data_sjw != 5U) || (can_pin_route != 0U))
    {
        puts("Effective startup/CAN configuration failed");
        return 1;
    }
    for (fault = 1U; fault <= 4U; fault++)
    {
        ResetMock(fault);
        result = F24_CanClockInit();
        F24_CanClockGetDiagnostic(diagnostic);
        if ((result != expected[fault]) ||
            (diagnostic[0] != 0xC1U) || (diagnostic[1] != result) ||
            (F24_CanClockIsReady() != 0U) || (MDIV != 0U) ||
            (MCM0 != 0U) || (MCS != 0U) || (PLLON != 0U) ||
            (SELPLLS != 0U) || (MSTOP != 1U))
        {
            printf("Clock fault %u failed, result %u\n", fault, result);
            return 1;
        }
    }
    ResetMock(0U);
    if ((F24_CanClockInit() != F24_CAN_CLOCK_READY) ||
        (F24_CanClockIsReady() == 0U) || (MDIV != 1U) ||
        (PLLCTL != 0xE7U) || ((CKSEL & 0xE0U) != 0xE0U) ||
        (CMC != 0x51U))
    {
        puts("Normal clock selection failed");
        return 1;
    }
    if (F24_CanClockInit() != F24_CAN_CLOCK_READY)
    {
        puts("Repeat initialization failed");
        return 1;
    }
    if (TestNominalTiming() != 0)
    {
        puts("250K/500K/1M timing or protected register writes failed");
        return 1;
    }
    CKSEL ^= 0x20U;
    if (F24_CanClockIsReady() != 0U)
    {
        puts("Wrong PLL input divider was accepted");
        return 1;
    }
    CKSEL ^= 0x20U;
    PLLCTL ^= 0x02U;
    if (F24_CanClockIsReady() != 0U)
    {
        puts("Wrong PLL multiplier was accepted");
        return 1;
    }
    PLLCTL ^= 0x02U;
    mock_pllsts.bits.b7 = 0U;
    if (F24_CanClockIsReady() != 0U)
    {
        puts("Clock loss was not detected");
        return 1;
    }
    if ((F24_CanTimingApply(2U) != 0U) ||
        (C0NCFGH != 0xAAAAU) || (C0NCFGL != 0xBBBBU))
    {
        puts("Timing write with invalid clock was accepted");
        return 1;
    }
    ResetMock(0U);
    mock_cmc = 0U;
    if ((F24_CanClockInit() != F24_CAN_CLOCK_BAD_STARTUP) ||
        (CMC != 0U) || (MSTOP != 1U))
    {
        puts("Locked incorrect CMC mode was not rejected");
        return 1;
    }
    ResetMock(0U);
    MDIV = 1U;
    if (F24_CanClockInit() != F24_CAN_CLOCK_BAD_STARTUP)
    {
        puts("Unexpected startup clock was accepted");
        return 1;
    }
    puts("F24 20 MHz clock, locked CMC, startup/CAN timing, timeout/fallback tests passed");
    return 0;
}
