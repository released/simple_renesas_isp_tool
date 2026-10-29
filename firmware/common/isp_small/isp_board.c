/* Project/API reading notes: Small-RL78 board backend.
 * The selected UART/IICA data path is separate from UART0 text Log.
 * Interrupt handlers capture bounded data/state; foreground calls own protocol processing and
 * deferred actions.
 */
#include "platform.h"
#include "isp_rl78_board.h"
#if defined(ISP_SCFG_REQUIRED) && !defined(ISP_SCFG_CPU_HZ)
#error The SC-owned project must preinclude its validated peripheral header.
#endif
#if defined(ISP_SCFG_CPU_HZ)
#define ISP_UART_CLOCK_HZ (ISP_SCFG_CPU_HZ / 2UL)
#define ISP_LOG_SDR ISP_SCFG_LOG_SDR
#define ISP_HOCO_DIVIDE ISP_SCFG_HOCO_DIVIDE
#define ISP_TIMER_RELOAD ISP_SCFG_TIMER_RELOAD
#elif defined(ISP_MCU_G16)
#define ISP_UART_CLOCK_HZ 8000000UL
#define ISP_LOG_SDR 0x4400U
#define ISP_HOCO_DIVIDE 1U
#define ISP_TIMER_RELOAD 15999U
#else
#define ISP_UART_CLOCK_HZ 16000000UL
#define ISP_LOG_SDR 0x8800U
#define ISP_HOCO_DIVIDE 0U
#define ISP_TIMER_RELOAD 31999U
#endif
#if !defined(ISP_I2C)
#include "isp_uart_timing.h"
#endif

/* G16 uses HOCO16 and G22 HOCO32. UART0 uses SAU0 channels0/1,
 * UART1 uses channels2/3; SPS divides the respective CPU clock by two.
 * The UART0 text route and UART1 binary route are physically independent.
 * The board configuration documents the exact header pins and IRQ numbers.
 */
static volatile unsigned short g_tick;
static volatile unsigned char g_head, g_tail;
static volatile unsigned char g_rx[128];
static volatile unsigned short g_errors, g_drops;
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
static unsigned short g_tx_errors;
static volatile unsigned char g_rx_peak;
void IspUartDiagnostics(unsigned char *bytes)
{
    unsigned char saved;
    saved = SRMK1;
    SRMK1 = 1U;
    bytes[0] = 1U;
    bytes[1] = 1U;
    bytes[2] = 0x0FU;
    bytes[3] = 0U;
    IspWrite32(bytes + 4U, g_errors);
    IspWrite32(bytes + 8U, g_drops);
    IspWrite32(bytes + 12U, g_tx_errors);
    IspWrite32(bytes + 16U, g_rx_peak);
    IspWrite32(bytes + 20U, (unsigned char)((g_head - g_tail) & 127U));
    SRMK1 = saved;
}
#endif

#pragma interrupt IspUartRxIsr
void IspUartRxIsr(void)
{
    unsigned char value, next, error;
    value = RXD1;
    error = (unsigned char)(SSR03 & 7U);
    if (error)
    {
        SIR03 = error;
        if (g_errors != 65535U)
        {
            ++g_errors;
        }
        return;
    }
    next = (unsigned char)((g_head + 1U) & 127U);
    if (next == g_tail)
    {
        if (g_drops != 65535U)
        {
            ++g_drops;
        }
        return;
    }
    g_rx[g_head] = value;
    g_head = next;
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
    next = (unsigned char)((g_head - g_tail) & 127U);
    if (next > g_rx_peak)
    {
        g_rx_peak = next;
    }
#endif
}
#pragma interrupt IspTimerIsr
void IspTimerIsr(void)
{
    ++g_tick;
}
unsigned short IspTick(void)
{
    return g_tick;
}
unsigned char IspUartGet(unsigned char *value)
{
    if (g_tail == g_head)
    {
        return 0U;
    }
    *value = g_rx[g_tail];
    g_tail = (unsigned char)((g_tail + 1U) & 127U);
    return 1U;
}
unsigned char IspUartSend(const unsigned char *data, unsigned short size)
{
    unsigned short at;
    unsigned long budget;
    for (at = 0U; at < size; ++at)
    {
        STIF1 = 0U;
        TXD1 = data[at];
        budget = 200000UL;
        while (!STIF1 && --budget)
        {
            BSP_NOP();
        }
        if (!budget)
        {
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
            if (g_tx_errors != 65535U)
            {
                ++g_tx_errors;
            }
#endif
            return 0U;
        }
    }
    STIF1 = 0U;
    return 1U;
}
void IspLog(const char __far *text)
{
    unsigned long budget;
    while (*text)
    {
        STIF0 = 0U;
        TXD0 = (unsigned char)*text++;
        budget = 200000UL;
        while (!STIF0 && --budget)
        {
            BSP_NOP();
        }
        if (!budget)
        {
            return;
        }
    }
    STIF0 = 0U;
}
void IspUartStop(void)
{
    SRMK1 = 1U;
    STMK1 = 1U;
    ST0 |= 15U;
    SRIF1 = 0U;
    STIF1 = 0U;
}
void IspTimerStop(void)
{
    TMMK01 = 1U;
    TT0 = 2U;
    TMIF01 = 0U;
}
void IspHeartbeat(void)
{
    static unsigned short last;
    if ((unsigned short)(g_tick - last) >= 500U)
    {
        last = g_tick;
#if defined(ISP_MCU_G16)
        P1 ^= 0x0CU;
#else
        P6 ^= 0x0CU;
#endif
    }
}
unsigned char IspBoardInit(void)
{
    unsigned short delay;
    BSP_DI();
    HIOSTOP = 0U;
    for (delay = 0U; delay < 2000U; ++delay)
    {
        BSP_NOP();
    }
    HOCODIV = ISP_HOCO_DIVIDE;
    CKC = 0U;
    SAU0EN = 1U;
    TAU0EN = 1U;
    BSP_NOP();
    BSP_NOP();
    ST0 = 15U;
    SPS0 = 0x0011U;
    STMK0 = 1U;
    SRMK0 = 1U;
    STMK1 = 1U;
    SRMK1 = 1U;
    SREMK0 = 1U;
    SREMK1 = 1U;
    SMR00 = 0x0022U;
    SCR00 = 0x8097U;
    SDR00 = ISP_LOG_SDR;
    SMR01 = 0x0122U;
    SCR01 = 0x4097U;
    SDR01 = ISP_LOG_SDR;
    SMR02 = 0x0022U;
    SCR02 = 0x8097U;
    SMR03 = 0x0122U;
    SCR03 = 0x4097U;
#if !defined(ISP_I2C)
    SDR02 = (unsigned short)ISP_UART_SDR;
    SDR03 = (unsigned short)ISP_UART_SDR;
#else
    SDR02 = ISP_LOG_SDR;
    SDR03 = ISP_LOG_SDR;
#endif
    SIR01 = 7U;
    SIR03 = 7U;
    NFEN0 |= 5U;
    SO0 |= 5U;
    SOL0 &= 0xFFFAU;
    SOE0 |= 5U;
#if defined(ISP_MCU_G16)
    /* PIOR21:20=10: UART0 TX P03/RX P04; PIOR31:30=10: UART1 P11/P10. */
    PIOR2 = (unsigned char)((PIOR2 & 0xFCU) | 2U);
    PIOR3 = (unsigned char)((PIOR3 & 0xFCU) | 2U);
    PMC0 &= 0xE7U;
    TSSEL0 &= 0xE7U;
    P0 |= 8U;
    PM0 = (unsigned char)((PM0 & 0xF7U) | 0x10U);
    P1 |= 2U;
    PM1 = (unsigned char)((PM1 & 0xF1U) | 1U);
    P1 |= 0x0CU;
#else
    /* Default routes: UART0 TX P12/RX P11; UART1 TX P00/RX P01. */
    P1 |= 4U;
    PM1 = (unsigned char)((PM1 & 0xFBU) | 2U);
    P0 |= 1U;
    PM0 = (unsigned char)((PM0 & 0xFEU) | 2U);
    P6 |= 0x0CU;
    PM6 &= 0xF3U;
#endif
    g_head = 0U;
    g_tail = 0U;
    STIF0 = 0U;
    SRIF0 = 0U;
    STIF1 = 0U;
    SRIF1 = 0U;
    SRPR11 = 1U;
    SRPR01 = 1U;
#if defined(ISP_I2C)
    /* I2C owns the update link; UART0 alone remains diagnostic text. */
    SS0 = 3U;
    SRMK1 = 1U;
#else
    SS0 = 15U;
    SRMK1 = 0U;
#endif
    TT0 = 2U;
    TMMK01 = 1U;
    TMIF01 = 0U;
    TPS0 = 0U;
    TMR01 = 0U;
    TDR01 = ISP_TIMER_RELOAD;
    TMPR101 = 1U;
    TMPR001 = 1U;
    TOE0 &= 0xFFFDU;
    TMMK01 = 0U;
    TS0 = 2U;
    BSP_EI();
    return 1U;
}
void IspBoardReset(void)
{
    BSP_DI();
    IAWCTL |= 0x80U;
    *(volatile unsigned char __far *)0UL = 0U;
    for (;;)
    {
        BSP_NOP();
    }
}
void boot_hdwinit(void)
{
    *(volatile unsigned char __far *)BOOT_APP_SHARED_RAM_ADDRESS = BOOT_APP_RAM_OWNER_BOOT;
}
void hdwinit(void)
{
}
