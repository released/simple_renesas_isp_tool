/* Actual small-device board initialization with modeled register storage. */
#include <assert.h>
#include <stdio.h>
static unsigned char HIOSTOP,HOCODIV,CKC,SAU0EN,TAU0EN;
static unsigned char STMK0,SRMK0,STMK1,SRMK1,SREMK0,SREMK1;
static unsigned char STIF0,SRIF0,STIF1,SRIF1,SRPR11,SRPR01;
static unsigned char P0,PM0,P1,PM1,P6,PM6,PIOR2,PIOR3,PMC0,TSSEL0,NFEN0;
static unsigned char TMMK01,TMIF01,TMPR101,TMPR001,g_head,g_tail;
static unsigned short ST0,SPS0,SMR00,SCR00,SDR00,SMR01,SCR01,SDR01;
static unsigned short SMR02,SCR02,SDR02,SMR03,SCR03,SDR03,SIR01,SIR03;
static unsigned short SO0,SOL0,SOE0,SS0,TT0,TPS0,TMR01,TDR01,TOE0,TS0;
#define BSP_NOP() ((void)0)
#define BSP_DI() ((void)0)
#define BSP_EI() ((void)0)
#define ISP_LOG_SDR ISP_SCFG_LOG_SDR
#define ISP_HOCO_DIVIDE ISP_SCFG_HOCO_DIVIDE
#define ISP_TIMER_RELOAD ISP_SCFG_TIMER_RELOAD
#include "actual_scfg_board_init.inc"
int main(void)
{
    g_head=11U;g_tail=17U;
    assert(IspBoardInit());
    assert(!g_head&&!g_tail&&HOCODIV==ISP_SCFG_HOCO_DIVIDE&&CKC==0U);
    assert(SPS0==0x11U&&SDR00==ISP_SCFG_LOG_SDR&&SDR01==ISP_SCFG_LOG_SDR);
    assert(TDR01==ISP_SCFG_TIMER_RELOAD&&TPS0==0U&&TS0==2U&&!TMMK01);
#if defined(ISP_I2C)
    assert(SS0==3U&&SRMK1==1U);
#else
    assert(SS0==15U&&SRMK1==0U&&SDR02==ISP_UART_SDR&&SDR03==ISP_UART_SDR);
#endif
#if defined(ISP_MCU_G16)
    assert(HOCODIV==1U&&TDR01==15999U&&SDR00==0x4400U);
    assert((PIOR2&3U)==2U&&(PIOR3&3U)==2U);
    assert((PM0&0x18U)==0x10U&&(PM1&3U)==1U);
    (void)P6;(void)PM6;
#else
    assert(HOCODIV==0U&&TDR01==31999U&&SDR00==0x8800U);
    assert((PM0&3U)==2U&&(PM1&6U)==2U);
    (void)PIOR2;(void)PIOR3;(void)PMC0;(void)TSSEL0;
#endif
    puts("PASS actual G16/G22 board init: SC clocks/divisors/timer/pin routing and independent UART0; not physical timing.");
    return 0;
}
