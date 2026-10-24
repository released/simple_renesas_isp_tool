#include <stdio.h>
#include "bridge_uart.h"
#include "f24_device_uart_mock.h"
uint8_t SAU1EN, STMK1, SRMK1, STIF1, SRIF1;
uint8_t PIOR4, PMC7, P7, PM7, NFEN0;
uint8_t STPR11, STPR01, SRPR11, SRPR01, SDR10L, SDR11L;
uint16_t ST1, SPS1, SMR10, SCR10, SDR10, SMR11, SCR11, SDR11;
uint16_t SIR11, SSR11, SO1, SOL1, SOE1, SS1;
static uint8_t clock_ready = 1U;
static unsigned failures;
uint8_t F24_CanClockIsReady(void) { return clock_ready; }
void Bridge_UartTxIsr(void);
void Bridge_UartRxIsr(void);
static void Check(int condition, const char *name)
{
    if (!condition) { ++failures; printf("FAIL %s\n", name); }
}
int main(void)
{
    unsigned i;
    uint8_t data[3] = {0x12U,0x34U,0x56U};
    uint8_t value, stats[24];
    uint32_t baud, clock, actual, error;
    const uint32_t rates[] = {300UL,9600UL,19200UL,38400UL,57600UL,115200UL,
        123456UL,230400UL,460800UL,921600UL,1000000UL,2000000UL};
    for (i=0U;i<sizeof(rates)/sizeof(rates[0]);++i) {
        baud=rates[i];
        Check(Bridge_UartBackend_Open(0U,baud)==0U,"numeric baud accepted");
        clock=40000000UL>>(SPS1&15U);
        actual=clock/(2UL*((uint32_t)(SDR10>>9)+1UL));
        error=actual>baud?actual-baud:baud-actual;
        Check(SDR10==SDR11&&(SDR10&0x1FFU)==0U&&(SDR10>>9)>=2U&&
            error*100UL<=baud*2UL,"actual register divisor/error bound");
    }
    Check(Bridge_UartBackend_Open(0U,0UL)==3U&&Bridge_UartBackend_Open(0U,299UL)==3U&&
        Bridge_UartBackend_Open(0U,2000001UL)==3U&&Bridge_UartBackend_Open(0U,1500000UL)==3U,
        "invalid/range/inaccurate baud rejected");
    Check(Bridge_UartBackend_Open(1U,115200UL) == 3U,"invalid port");
    clock_ready = 0U;
    Check(Bridge_UartBackend_Open(0U,115200UL) == 10U,"clock gate");
    clock_ready = 1U; PIOR4 = 0x82U; PM7 = 0xFFU; PMC7 = 0xFFU;
    Check(Bridge_UartBackend_Open(0U,115200UL) == 0U,"open");
    Check(PIOR4 == 0x86U && (PM7 & 0x30U) == 0x20U &&
          (PMC7 & 0x10U) == 0U,"P74 TX/P75 RX redirection preserves other routes");
    Check(SDR10 == 0xAC00U && SDR11 == 0xAC00U && SPS1 == 0x11U,"40MHz baud timing");
    Check(Bridge_UartBackend_Begin(data,3U,1U) == 0U && SDR10L == 0x12U &&
          !STMK1 && !Bridge_UartBackend_TxIdle(),"first TX byte");
    Bridge_UartTxIsr(); Check(SDR10L == 0x34U,"TX IRQ second");
    Bridge_UartTxIsr(); Check(SDR10L == 0x56U,"TX IRQ third");
    Bridge_UartTxIsr(); Check(STMK1 && Bridge_UartBackend_TxIdle(),"TX complete only after final byte");
    for (i=0U;i<255U;++i) { SDR11L=(uint8_t)i; Bridge_UartRxIsr(); }
    SDR11L=0xFFU; Bridge_UartRxIsr();
    Check(Bridge_UartBackend_Error() == 8U,"ring overflow latched");
    for (i=0U;i<255U;++i) Check(Bridge_UartBackend_Get(&value) && value == (uint8_t)i,"ring keeps oldest");
    Check(!Bridge_UartBackend_Get(&value),"ring bounded");
    Check(Bridge_UartBackend_Begin(data,0U,0U) == 0U &&
          Bridge_UartBackend_Error() == 8U,"no-clear RX preserves prior loss evidence");
    Bridge_UartBackend_Begin(data,0U,1U);
    Check(!Bridge_UartBackend_Error(),"explicit clear resets transaction error");
    SSR11=5U; Bridge_UartRxIsr();
    Check(Bridge_UartBackend_Error() == 11U && SIR11 == 5U &&
          !Bridge_UartBackend_Get(&value),"UART error clears hardware and drops corrupt byte");
    SRMK1=1U; STMK1=0U; Bridge_UartBackend_Stats(stats);
    Check(SRMK1 == 1U && STMK1 == 0U,"counter snapshot restores masks");
    Check(stats[0] == 255U && stats[4] == 3U && stats[8] == 1U &&
          stats[12] == 1U && stats[16] == 255U,"RX/TX/error/drop/peak counters");
    Bridge_UartBackend_Abort();
    Check(STMK1 && SRMK1 && Bridge_UartBackend_TxIdle() &&
          !(SOE1 & 1U),"abort stops only UART1");
    SSR11=0U; Bridge_UartBackend_Begin(data,1U,1U);
    Check(!STMK1 && !SRMK1 && (SOE1 & 1U),"next transaction restarts UART1");
    if (!failures) puts("PASS actual F24 UART1 backend with register model; electrical/IRQ timing NOT RUN");
    return failures ? 1 : 0;
}
