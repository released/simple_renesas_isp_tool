/* Project/API reading notes: RL78 board backend for one selected ISP data interface plus an
 * independent text Log UART.
 * Keep interrupt queues/mailboxes bounded; the foreground runtime owns packet parsing, Flash
 * decisions and deferred reset actions.
 */
#if defined(ISP_MCU_F13)
#include "r_cg_macrodriver.h"
#include "f13_board_support.h"
#define ISP_NOP() NOP()
#define ISP_DIVISOR 0x8800U
#else
#include "platform.h"
#include "Config_TAU0_1.h"
#include "r_cg_tau_common.h"
#define DI() BSP_DI()
#define EI() BSP_EI()
#define ISP_NOP() BSP_NOP()
#if defined(ISP_SCFG_LOG_SDR)
#define ISP_DIVISOR ISP_SCFG_LOG_SDR
#else
#define ISP_DIVISOR 0xAC00U
#endif
extern unsigned char Device_ClockInit(void);
#endif
#include "isp_rl78_board.h"
#if defined(ISP_SCFG_REQUIRED) && !defined(ISP_SCFG_CPU_HZ)
#error The SC-owned project must preinclude its validated peripheral header.
#endif
#if defined(ISP_SCFG_REQUIRED) && defined(ISP_MCU_F24) && !defined(F24_DEVICE_PREINCLUDE_H)
#error Keep the F24 startup clock override as well as the SC peripheral header.
#endif

#if defined(ISP_MCU_F13)
#define ISP_UART_CLOCK_HZ 16000000UL
#else
#if defined(ISP_SCFG_CPU_HZ)
#define ISP_UART_CLOCK_HZ (ISP_SCFG_CPU_HZ / 2UL)
#else
#define ISP_UART_CLOCK_HZ 20000000UL
#endif
#endif
#if !defined(ISP_I2C)
#include "isp_uart_timing.h"
#endif

static volatile unsigned short g_tick;
static volatile unsigned char g_head, g_tail;
static volatile unsigned char g_rx[256];
static volatile unsigned short g_rx_errors, g_rx_drops;
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
    IspWrite32(bytes + 4U, g_rx_errors);
    IspWrite32(bytes + 8U, g_rx_drops);
    IspWrite32(bytes + 12U, g_tx_errors);
    IspWrite32(bytes + 16U, g_rx_peak);
    IspWrite32(bytes + 20U, (unsigned char)(g_head - g_tail));
    SRMK1 = saved;
}
#endif

#pragma interrupt IspUartRxIsr
void IspUartRxIsr(void)
{
    unsigned char value, next, error;
    value = SDR11L;
    error = (unsigned char)(SSR11 & 7U);
    if (error)
    {
        SIR11 = error;
        if (g_rx_errors != 65535U)
        {
            ++g_rx_errors;
        }
        return;
    }
    next = (unsigned char)(g_head + 1U);
    if (next == g_tail)
    {
        if (g_rx_drops != 65535U)
        {
            ++g_rx_drops;
        }
        return;
    }
    g_rx[g_head] = value;
    g_head = next;
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
    next = (unsigned char)(g_head - g_tail);
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
    g_tail = (unsigned char)(g_tail + 1U);
    return 1U;
}
unsigned char IspUartSend(const unsigned char *data, unsigned short size)
{
    unsigned short i;
    unsigned long budget;
    for (i = 0U; i < size; ++i)
    {
        STIF1 = 0U;
        SDR10L = data[i];
        budget = 200000UL;
        while (!STIF1 && --budget)
        {
            ISP_NOP();
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
static void uart_init(void)
{
    SAU0EN = 1U;
#if !defined(ISP_I2C)
    SAU1EN = 1U;
#endif
    ISP_NOP();
    ISP_NOP();
    ISP_NOP();
    ISP_NOP();
    SPS0 = 0x0011U;
    ST0 |= 3U;
#if !defined(ISP_I2C)
    SPS1 = 0x0011U;
    ST1 |= 3U;
#endif
    STMK0 = 1U;
    SRMK0 = 1U;
    STMK1 = 1U;
    SRMK1 = 1U;
    STIF0 = 0U;
    SRIF0 = 0U;
    STIF1 = 0U;
    SRIF1 = 0U;
    SMR00 = 0x0022U;
    SCR00 = 0x8097U;
    SDR00 = ISP_DIVISOR;
    SMR01 = 0x0122U;
    SCR01 = 0x4097U;
    SDR01 = ISP_DIVISOR;
#if !defined(ISP_I2C)
    SMR10 = 0x0022U;
    SCR10 = 0x8097U;
    SDR10 = (unsigned short)ISP_UART_SDR;
    SMR11 = 0x0122U;
    SCR11 = 0x4097U;
    SDR11 = (unsigned short)ISP_UART_SDR;
#endif
    NFEN0 |= 5U;
    SIR01 = 7U;
    SIR11 = 7U;
    SO0 |= 1U;
    SOL0 &= 0xFFFEU;
    SOE0 |= 1U;
#if !defined(ISP_I2C)
    SO1 |= 1U;
    SOL1 &= 0xFFFEU;
    SOE1 |= 1U;
#endif
    P1 |= 0x20U;
    PM1 &= 0xDFU;
    PM1 |= 0x40U;
#if !defined(ISP_I2C)
#if defined(ISP_MCU_F13)
    /* F13 EVB P11/P12 belong to the CAN transceiver. Keep it in standby;
     * PIOR42 routes UART1 to the independent P74 TX/P75 RX header pins. */
    P1 |= 4U;
    PM1 &= 0xFBU;
    PM1 |= 2U;
    PIOR4 |= 4U;
    P7 |= 0x10U;
    PM7 &= 0xEFU;
    PM7 |= 0x20U;
#else
    /* F24 UART1: TX P74, RX P75, as selected by the board configuration. */
    PIOR4 |= 4U; /* PIOR42=1; preserve CAN and other peripheral routes. */
    PMC7 &= 0xEFU;
    P7 |= 0x10U;
    PM7 &= 0xEFU;
    PM7 |= 0x20U;
#endif
#endif
    g_head = 0U;
    g_tail = 0U;
    SRPR11 = 1U;
    SRPR01 = 1U;
    SS0 |= 3U;
#if !defined(ISP_I2C)
    SS1 |= 3U;
    SRIF1 = 0U;
    SRMK1 = 0U;
#endif
}
void IspUartStop(void)
{
    SRMK1 = 1U;
    STMK1 = 1U;
    ST1 |= 3U;
    ST0 |= 3U;
    SRIF1 = 0U;
    STIF1 = 0U;
}
void IspTimerStop(void)
{
#if defined(ISP_MCU_F13)
    TRJMK0 = 1U;
    TRJCR0 = 0U;
    TRJIF0 = 0U;
#else
    R_Config_TAU0_1_Stop();
#endif
}
unsigned char IspBoardInit(void)
{
    unsigned char ready;
#if defined(ISP_MCU_F13)
    ready = (unsigned char)(F13_BoardClockStart() == F13_BOARD_CLOCK_OK);
    if (!ready)
    {
        return 0U;
    }
    TRJ0EN = 1U;
    TRJCR0 = 0U;
    TRJMK0 = 1U;
    TRJIF0 = 0U;
    TRJPR10 = 1U;
    TRJPR00 = 1U;
    TRJMR0 = 0U;
    TRJIOC0 = 0U;
    TRJ0 = 0x7CFFU;
    TRJMK0 = 0U;
    TRJCR0 = 1U;
#else
    ready = Device_ClockInit();
    if (!ready)
    {
        return 0U;
    }
    R_Config_TAU0_1_Start();
#endif
    P6 |= 0xC0U;
    PM6 &= 0x3FU;
    uart_init();
    EI();
    return 1U;
}
void IspHeartbeat(void)
{
    static unsigned short last;
    if ((unsigned short)(g_tick - last) >= 500U)
    {
        last = g_tick;
        P6 ^= 0xC0U;
    }
}
void IspLog(const char __far *text)
{
    unsigned long budget;
    while (*text)
    {
        STIF0 = 0U;
        SDR00L = (unsigned char)*text++;
        budget = 200000UL;
        while (!STIF0 && --budget)
        {
            ISP_NOP();
        }
        if (!budget)
        {
            return;
        }
    }
    STIF0 = 0U;
}
void IspBoardReset(void)
{
    DI();
    IAWCTL |= 0x80U;
    *(volatile unsigned char __far *)0UL = 0U;
    for (;;)
    {
        ISP_NOP();
    }
}
#if defined(ISP_MCU_F13)
void boot_hdwinit(void)
{
    *(volatile unsigned char __far *)BOOT_APP_SHARED_RAM_ADDRESS = BOOT_APP_RAM_OWNER_BOOT;
    F13_BoardClockPrepare();
}
void hdwinit(void)
{
    F13_BoardClockPrepare();
}
#else
void R_Config_TAU0_1_Create_UserInit(void)
{
}
void R_Systeminit(void)
{
#if !defined(ISP_DEVICE_APP)
    *(volatile unsigned char __far *)BOOT_APP_SHARED_RAM_ADDRESS = BOOT_APP_RAM_OWNER_BOOT;
#endif
    R_TAU0_Create();
}
#endif
