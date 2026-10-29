#ifdef BRIDGE_UART_BACKEND_TEST
#include "f24_device_uart_mock.h"
#else
#include "r_cg_macrodriver.h"
#include "f24_can_clock.h"
#endif
#include "bridge_uart.h"

/* UART1/SAU1 is independent of binary Host Link UART0/SAU0.
 * CPU40MHz;115200 uses CK00=fCLK/2 and SDR[15:9]=86,actual114942.
 * R01UH0944 section15.3.5: baud=CK00/(2*(SDR[15:9]+1));UART requires
 * SDR[15:9]>=2. Reject divisors with nominal error greater than2percent.
 * PIOR42 selects P74 TX/P75 RX. No generated UART1 component owns SAU1. */
static volatile uint8_t g_uart_rx[256];
static volatile uint8_t g_uart_head, g_uart_tail, g_uart_error, g_uart_busy;
static uint8_t g_uart_open;
static const uint8_t *g_uart_tx;
static volatile uint16_t g_uart_tx_at;
static uint16_t g_uart_tx_length;
static volatile uint32_t g_uart_counts[6];

static void UartIncrement(volatile uint32_t *value)
{
    if (*value != 0xFFFFFFFFUL)
    {
        ++*value;
    }
}
#pragma interrupt Bridge_UartTxIsr
void Bridge_UartTxIsr(void)
{
    if (!g_uart_busy)
    {
        STMK1 = 1U;
        return;
    }
    UartIncrement(&g_uart_counts[1]);
    if (g_uart_tx_at < g_uart_tx_length)
    {
        SDR10L = g_uart_tx[g_uart_tx_at++];
    }
    else
    {
        g_uart_busy = 0U;
        STMK1 = 1U;
    }
}
#pragma interrupt Bridge_UartRxIsr
void Bridge_UartRxIsr(void)
{
    uint8_t value, error, next, depth;
    value = SDR11L;
    error = (uint8_t)(SSR11 & 7U);
    if (error)
    {
        SIR11 = error;
        if (!g_uart_error)
        {
            g_uart_error = 11U;
        }
        UartIncrement(&g_uart_counts[2]);
        return;
    }
    next = (uint8_t)(g_uart_head + 1U);
    if (next == g_uart_tail)
    {
        if (!g_uart_error)
        {
            g_uart_error = 8U;
        }
        UartIncrement(&g_uart_counts[3]);
        return;
    }
    g_uart_rx[g_uart_head] = value;
    g_uart_head = next;
    UartIncrement(&g_uart_counts[0]);
    depth = (uint8_t)(next - g_uart_tail);
    if ((uint32_t)depth > g_uart_counts[4])
    {
        g_uart_counts[4] = depth;
    }
}
uint8_t Bridge_UartBackend_Open(uint8_t port, uint32_t baud)
{
    uint8_t prescaler, found;
    uint32_t clock, divisor, actual, error;
    uint16_t timing;
    if (port != 0U || baud < DEVICE_UART_BAUD_MIN || baud > DEVICE_UART_BAUD_MAX)
    {
        return 3U;
    }
    if (!F24_CanClockIsReady())
    {
        return 10U;
    }
    clock = 40000000UL;
    found = 0U;
    timing = 0U;
    for (prescaler = 0U; prescaler < 16U; ++prescaler)
    {
        divisor = (clock + baud) / (2UL * baud);
        if (divisor >= 3UL && divisor <= 128UL)
        {
            actual = clock / (2UL * divisor);
            error = actual > baud ? actual - baud : baud - actual;
            if (error * 100UL <= baud * 2UL)
            {
                timing = (uint16_t)((divisor - 1UL) << 9);
                found = 1U;
                break;
            }
        }
        clock >>= 1;
    }
    if (!found)
    {
        return 3U; /* No hardware changes on rejected settings. */
    }
    SAU1EN = 1U;
    NOP();
    NOP();
    NOP();
    NOP();
    STMK1 = 1U;
    SRMK1 = 1U;
    ST1 |= 3U;
    SPS1 = (uint16_t)(0x0010U | prescaler);
    SMR10 = 0x0022U;
    SCR10 = 0x8097U;
    SDR10 = timing;
    SMR11 = 0x0122U;
    SCR11 = 0x4097U;
    SDR11 = timing;
    NFEN0 |= 4U;
    SIR11 = 7U;
    PIOR4 |= 4U;
    PMC7 &= 0xEFU;
    P7 |= 0x10U;
    PM7 &= 0xEFU;
    PM7 |= 0x20U;
    SO1 |= 1U;
    SOL1 &= 0xFFFEU;
    SOE1 |= 1U;
    STPR11 = 1U;
    STPR01 = 1U;
    SRPR11 = 1U;
    SRPR01 = 1U;
    g_uart_head = 0U;
    g_uart_tail = 0U;
    g_uart_busy = 0U;
    g_uart_error = 0U;
    STIF1 = 0U;
    SRIF1 = 0U;
    SS1 |= 3U;
    SRMK1 = 0U;
    g_uart_open = 1U;
    return 0U;
}
uint8_t Bridge_UartBackend_Begin(const uint8_t *tx, uint16_t length, uint8_t clear_rx)
{
    if (!g_uart_open || length > BRIDGE_UART_LIMIT)
    {
        return 10U;
    }
    if (g_uart_busy)
    {
        return 1U;
    }
    SRMK1 = 1U;
    if (clear_rx)
    {
        g_uart_tail = g_uart_head;
        SIR11 = 7U;
        SRIF1 = 0U;
        g_uart_error = 0U;
    }
    SRMK1 = 0U;
    /* Abort stops channels but preserves the validated configuration. */
    SO1 |= 1U;
    SOE1 |= 1U;
    SS1 |= 3U;
    if (length)
    {
        STMK1 = 1U;
        STIF1 = 0U;
        g_uart_tx = tx;
        g_uart_tx_length = length;
        g_uart_tx_at = 1U;
        g_uart_busy = 1U;
        SDR10L = tx[0];
        STMK1 = 0U;
    }
    return 0U;
}
uint8_t Bridge_UartBackend_Get(uint8_t *value)
{
    if (g_uart_tail == g_uart_head)
    {
        return 0U;
    }
    *value = g_uart_rx[g_uart_tail];
    g_uart_tail = (uint8_t)(g_uart_tail + 1U);
    return 1U;
}
uint8_t Bridge_UartBackend_TxIdle(void)
{
    return (uint8_t)!g_uart_busy;
}
uint8_t Bridge_UartBackend_Error(void)
{
    return g_uart_error;
}
void Bridge_UartBackend_Abort(void)
{
    if (!g_uart_open)
    {
        return;
    }
    STMK1 = 1U;
    SRMK1 = 1U;
    ST1 |= 3U;
    SOE1 &= 0xFFFEU;
    SO1 |= 1U;
    STIF1 = 0U;
    SRIF1 = 0U;
    SIR11 = 7U;
    g_uart_busy = 0U;
    g_uart_tail = g_uart_head;
    g_uart_error = 0U;
    UartIncrement(&g_uart_counts[5]);
}
void Bridge_UartBackend_Stats(uint8_t *output)
{
    uint8_t tx_mask, rx_mask, i, j;
    uint32_t value;
    tx_mask = STMK1;
    rx_mask = SRMK1;
    STMK1 = 1U;
    SRMK1 = 1U;
    for (i = 0U; i < 6U; ++i)
    {
        value = g_uart_counts[i];
        for (j = 0U; j < 4U; ++j)
        {
            *output++ = (uint8_t)value;
            value >>= 8;
        }
    }
    SRMK1 = rx_mask;
    STMK1 = tx_mask;
}
