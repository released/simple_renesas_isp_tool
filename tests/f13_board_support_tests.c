#include <stdio.h>
#include "f13_board_mock.h"
#include "../../../_Avery_Renesas/CS+/RL78_F13_Boot_loader_CAN/f13_board_support.h"

volatile f13_mock_reg8_t f13_pllctl, f13_ckc;
volatile unsigned char CMC, MSTOP, HIOSTOP, SELLOSC, MDIV, XTSTOP;
volatile unsigned char RTCCL, TRD_CKSEL, OSTS, OSTC, PLLSTS;
volatile unsigned char P1, PM1, PSRSEL, CAN0EN;
volatile unsigned char CAN0TRMMK, CAN0CFRMK, CAN0ERRMK, CANGRFRMK, CANGERRMK;
volatile unsigned short GSTS, C0CTRL, C0STSL, GCTRL;

enum
{
    NORMAL, X1_FAIL, MAIN_FAIL, PLL_FAIL, SELECT_FAIL, FALLBACK_FAIL,
    RAM_FAIL, CHANNEL_FAIL, GLOBAL_FAIL
};
static int scenario;
static int failures;
static unsigned long nop_count;

void F13MockNop(void)
{
    nop_count++;
    if ((MSTOP == 0U) && (scenario != X1_FAIL) && (scenario != FALLBACK_FAIL))
    {
        OSTC = 0xFFU;
    }
    if ((MCM0 != 0U) && (scenario != MAIN_FAIL))
    {
        CKC |= 0x20U;
    }
    else if ((MCM0 == 0U) && (scenario != FALLBACK_FAIL))
    {
        CKC &= 0xDFU;
    }
    if (PLLON == 0U)
    {
        PLLSTS = 0U;
    }
    else if (scenario != PLL_FAIL)
    {
        PLLSTS = 0x80U;
        if ((SELPLL != 0U) && (scenario != SELECT_FAIL))
        {
            PLLSTS |= 0x08U;
        }
    }
    if (CAN0EN != 0U)
    {
        if (scenario != RAM_FAIL)
        {
            GSTS &= 0xFFF7U;
        }
        if (((C0CTRL & 7U) == 1U) && (scenario != CHANNEL_FAIL))
        {
            C0STSL = 1U;
        }
        if (((GCTRL & 7U) == 1U) && (scenario != GLOBAL_FAIL))
        {
            GSTS = 1U;
        }
    }
}

static void Check(int condition, const char * name)
{
    if (!condition)
    {
        printf("FAIL: F13 board %s\n", name);
        failures++;
    }
}

static void ResetMock(void)
{
    PLLCTL = 0U;
    CKC = 0U;
    CMC = 0U;
    MSTOP = 1U;
    MDIV = 1U;
    OSTS = OSTC = PLLSTS = 0U;
    P1 = 0U;
    PM1 = PSRSEL = 0xFFU;
    CAN0EN = 0U;
    GSTS = C0CTRL = C0STSL = GCTRL = 0U;
    scenario = NORMAL;
    nop_count = 0UL;
}

int TestF13BoardSupport(void)
{
    unsigned char status;
    unsigned long before;
    int fault;
    static const unsigned char expected[] = {0U, 2U, 3U, 4U, 5U, 6U};

    failures = 0;
    ResetMock();
    F13_BoardClockPrepare();
    Check(CMC == 0x40U && MSTOP == 1U && MDIV == 1U && nop_count == 0UL,
          "pre-main preparation has no oscillator wait");
    status = F13_BoardClockStart();
    Check(status == 0U && F13_BoardClockReady() && MDIV == 0U &&
          PLLCTL == 0x47U && (PLLSTS & 0x88U) == 0x88U,
          "4MHz /2 x16 -> CPU32 and CAN16 benchmark clock");
    before = nop_count;
    F13_BoardClockPrepare();
    Check(F13_BoardClockStart() == 0U && before == nop_count && CMC == 0x40U,
          "boot-to-app preserves a valid PLL and locked CMC");
    Check(F13_BoardCanPrepare() && (P1 & 5U) == 5U && (PM1 & 7U) == 2U &&
          (C0STSL & 7U) == 1U && (GSTS & 7U) == 1U && CAN0TRMMK,
          "CAN pins and reset states established before driver configuration");
    C0CTRL = GCTRL = C0STSL = GSTS = 0U;
    Check(F13_BoardCanPrepare() && C0CTRL == 1U && GCTRL == 1U,
          "inherited operating CAN is reset at app startup");

    for (fault = X1_FAIL; fault <= FALLBACK_FAIL; fault++)
    {
        ResetMock();
        F13_BoardClockPrepare();
        scenario = fault;
        if (fault == FALLBACK_FAIL)
        {
            CKC |= 0x20U;
        }
        Check(F13_BoardClockStart() == expected[fault], "bounded clock failure stage");
        Check(!F13_BoardClockReady(), "failed clock cannot qualify CAN or FSL");
        if (fault != FALLBACK_FAIL)
        {
            Check(MDIV == 1U && !PLLON && !MCM0 && MSTOP == 1U,
                  "clock failure restores HOCO64/2 startup CPU");
        }
        Check(!F13_BoardCanPrepare() && !CAN0EN && (P1 & 4U),
              "failed clock leaves CAN disabled and transceiver silent");
    }
    ResetMock();
    Check(F13_BoardClockStart() == F13_BOARD_CLOCK_CONFIG_ERROR,
          "incorrect first CMC write is rejected rather than rewritten");
    for (fault = RAM_FAIL; fault <= GLOBAL_FAIL; fault++)
    {
        ResetMock();
        F13_BoardClockPrepare();
        Check(F13_BoardClockStart() == 0U, "setup before CAN fault");
        scenario = fault;
        GSTS = (fault == RAM_FAIL) ? 8U : 0U;
        Check(!F13_BoardCanPrepare() && (P1 & 4U),
              "RAM/channel/global timeout is bounded and keeps S HIGH");
    }
    ResetMock();
    F13_BoardClockPrepare();
    Check(F13_BoardClockStart() == 0U, "setup before divider check");
    MDIV = 1U;
    Check(!F13_BoardClockReady() && !F13_BoardCanPrepare(),
          "wrong CPU divider cannot silently change nominal CAN/FSL timing");
    if (failures == 0)
    {
        printf("F13 benchmark clock, handoff, CAN reset and timeout tests passed\n");
    }
    return failures;
}
