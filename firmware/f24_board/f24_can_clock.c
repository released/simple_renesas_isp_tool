#include "platform.h"
#include "f24_can_clock.h"

#define F24_X1_POLL_LIMIT (1000000UL)
#define F24_SWITCH_POLL_LIMIT (60000UL)

#if (BSP_CFG_MAINCLK_SOURCE != 0) || (BSP_CFG_FMP_DIVIDE != 0) || (BSP_CFG_PLL_MODE != 0) ||       \
    (BSP_CFG_PLL_OPERATION != 0) || (BSP_CFG_HISYSCLK_SOURCE != 1) ||                              \
    (BSP_CFG_FMX_HZ != 20000000) || (BSP_CFG_HISYSCLK_OPERATION != 1) ||                           \
    (BSP_CFG_FCLK_SOURCE != 0)
#error F24_CAN_bridge_requires_HOCO_startup_before_bounded_runtime_clock_setup
#endif

static uint8_t g_clock_status = F24_CAN_CLOCK_NOT_STARTED;

static uint8_t wait_register(volatile __near uint8_t *reg, uint8_t mask, uint8_t expected,
                             uint32_t limit)
{
    uint32_t count;

    for (count = 0UL; count < limit; count++)
    {
        if ((*reg & mask) == expected)
        {
            return 1U;
        }
        BSP_NOP();
    }
    return 0U;
}

static uint8_t clock_fail(uint8_t status)
{
    /* Restore clock-through mode before selecting HOCO as fMAIN. */
    SELPLL = 0U;
    if (wait_register(&PLLSTS, 0x08U, 0U, F24_SWITCH_POLL_LIMIT) == 0U)
    {
        g_clock_status = F24_CAN_CLOCK_FALLBACK_TIMEOUT;
        return g_clock_status;
    }
    /* The manual prohibits changing MCM0 while PLLON is set, even after
     * selecting clock-through mode. Stop PLL before selecting HOCO. */
    PLLON = 0U;
    MCM0 = 0U;
    if (wait_register(&CKC, 0x20U, 0U, F24_SWITCH_POLL_LIMIT) == 0U)
    {
        g_clock_status = F24_CAN_CLOCK_FALLBACK_TIMEOUT;
        return g_clock_status;
    }
    MDIV = 0U;
    MSTOP = 1U;
    g_clock_status = status;
    return status;
}

uint8_t F24_CanClockIsReady(void)
{
    return (uint8_t)((g_clock_status == F24_CAN_CLOCK_READY) && (MDIV == 1U) && (MCS != 0U) &&
                     (CLS == 0U) && ((PLLSTS & 0x88U) == 0x88U) && (MSTOP == 0U) &&
                     ((CMC & 0xC1U) == 0x41U) && ((CKSEL & 0xE0U) == 0xE0U) && (PLLCTL == 0xE7U));
}

uint8_t F24_CanClockInit(void)
{
    uint8_t count;

    if (F24_CanClockIsReady() != 0U)
    {
        return F24_CAN_CLOCK_READY;
    }
    if ((MCS != 0U) || (CLS != 0U) || (MDIV != 0U) || (SELPLLS != 0U) || (PLLON != 0U) ||
        ((CMC & 0xC1U) != 0x41U))
    {
        g_clock_status = F24_CAN_CLOCK_BAD_STARTUP;
        return g_clock_status;
    }

    HIOSTOP = 0U;
    MSTOP = 1U;
    /* CMC is write-once after reset. BSP has already selected 20 MHz X1
     * mode (OSCSEL=1, EXCLK=0, AMPH=1); never try to rewrite it here. */
    OSTS = 7U; /* 2^18 startup selection; same as the supplied CAN examples. */
    MSTOP = 0U;
    if (wait_register(&OSTC, 0xFFU, 0xFFU, F24_X1_POLL_LIMIT) == 0U)
    {
        return clock_fail(F24_CAN_CLOCK_X1_TIMEOUT);
    }

    MCM0 = 1U;
    if (wait_register(&CKC, 0x20U, 0x20U, F24_SWITCH_POLL_LIMIT) == 0U)
    {
        return clock_fail(F24_CAN_CLOCK_MAIN_TIMEOUT);
    }

    /* Set the input-divider pair atomically; binary 01 is prohibited. */
    CKSEL = (uint8_t)((CKSEL & 0x1FU) | 0xE0U);
    /* 20 MHz /4 x16, no output division, 80 MHz PLL output. The manual
     * permits this combination for a 20 MHz input. Lock wait is 1024/fMAIN
     * = 51.2 us, satisfying the minimum 40 us lock-wait period.
     * PLLON and SELPLL remain clear while programming PLL parameters. */
    PLLCTL = 0xE2U;
    for (count = 0U; count < 8U; count++)
    {
        BSP_NOP();
    }
    PLLON = 1U;
    if (wait_register(&PLLSTS, 0x80U, 0x80U, F24_SWITCH_POLL_LIMIT) == 0U)
    {
        return clock_fail(F24_CAN_CLOCK_PLL_TIMEOUT);
    }

    /* Divide before selecting PLL so the CPU never exceeds 40 MHz. */
    MDIV = 1U;
    SELPLL = 1U;
    if (wait_register(&PLLSTS, 0x88U, 0x88U, F24_SWITCH_POLL_LIMIT) == 0U)
    {
        return clock_fail(F24_CAN_CLOCK_SELECT_TIMEOUT);
    }
    g_clock_status = F24_CAN_CLOCK_READY;
    return g_clock_status;
}

void F24_CanClockGetDiagnostic(uint8_t *payload)
{
    payload[0] = 0xC1U;
    payload[1] = g_clock_status;
    payload[2] = MDIV;
    payload[3] = CKC;
    payload[4] = PLLCTL;
    payload[5] = PLLSTS;
    payload[6] = CMC;
    payload[7] = OSTC;
}
