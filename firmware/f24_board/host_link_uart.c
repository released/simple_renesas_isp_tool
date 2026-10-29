#ifdef HOST_LINK_UART_TEST
#include "f24_uart_mock.h"
#else
#include "platform.h"
#include "r_cg_macrodriver.h"
#include "Config_UART0.h"
#include "f24_runtime_hooks.h"
#endif
#include "host_link_uart.h"

#define UART_ERROR_OVERRUN (0x01U)
#define UART_ERROR_PARITY (0x02U)
#define UART_ERROR_FRAMING (0x04U)

static uint8_t g_rx_buffer[HOST_LINK_UART_RX_CAPACITY];
static volatile uint8_t g_rx_head;
static volatile uint8_t g_rx_tail;

static uint8_t g_tx_buffer[HOST_LINK_UART_TX_CAPACITY];
static volatile uint8_t g_tx_head;
static volatile uint8_t g_tx_tail;
static volatile uint8_t g_tx_busy;
static volatile uint16_t g_tx_active_length;

static volatile host_link_uart_stats_t g_uart_stats;
#ifndef BRIDGE_SPLIT_BOOT
static uint32_t g_baud, g_next_baud;
static uint16_t g_baud_tick;
static uint8_t g_baud_phase, g_rate_changed;

static uint16_t baud_divisor(uint32_t baud)
{
    /* CKM0 remains fCLK/2 = 20 MHz. Do not change a shared SAU clock. */
    if (baud == 460800UL)
    {
        return 0x2A00U;
    }
    if (baud == 921600UL)
    {
        return 0x1400U;
    }
    return 0xAC00U;
}

static void apply_baud(uint32_t baud)
{
    R_Config_UART0_Stop();
    SDR00 = baud_divisor(baud);
    SDR01 = baud_divisor(baud);
    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_tx_head = 0U;
    g_tx_tail = 0U;
    g_tx_busy = 0U;
    g_tx_active_length = 0U;
    g_baud = baud;
    g_rate_changed = 1U;
    R_Config_UART0_Start();
}

void HostLinkUart_BaudInfo(uint32_t *nominal, uint32_t *actual)
{
    *nominal = g_baud;
    *actual = 20000000UL / (2UL * ((baud_divisor(g_baud) >> 9) + 1UL));
}
uint8_t HostLinkUart_PrepareBaud(uint32_t baud)
{
    if (baud != HL_BAUD_DEFAULT && baud != 460800UL && baud != 921600UL)
    {
        return 3U;
    }
    if (g_baud_phase != 0U)
    {
        return 1U;
    }
    if (baud == g_baud)
    {
        return 0U;
    }
    g_next_baud = baud;
    g_baud_tick = get_tick();
    g_baud_phase = 1U; /* Prepared, but no ACK has been queued yet. */
    return 0U;
}
void HostLinkUart_ArmBaud(void)
{
    if (g_baud_phase == 1U)
    {
        g_baud_phase = 2U;
    }
}
uint8_t HostLinkUart_ConfirmBaud(uint32_t baud)
{
    if (g_baud_phase != 3U || baud != g_baud)
    {
        return 10U;
    }
    g_baud_phase = 0U;
    return 0U;
}
uint8_t HostLinkUart_BaudPending(void)
{
    return g_baud_phase;
}
uint8_t HostLinkUart_TakeRateChange(void)
{
    uint8_t changed;
    changed = g_rate_changed;
    g_rate_changed = 0U;
    return changed;
}

static void process_baud(void)
{
    uint16_t now, elapsed;
    if (g_baud_phase == 0U)
    {
        return;
    }
    now = get_tick();
    elapsed = (now >= g_baud_tick) ? (uint16_t)(now - g_baud_tick)
                                   : (uint16_t)(60000U - g_baud_tick + now);
    if (elapsed >= HL_BAUD_CONFIRM_MS)
    {
        apply_baud(HL_BAUD_DEFAULT);
        g_baud_phase = 0U;
    }
    else if (g_baud_phase == 2U && HostLinkUart_IsTxIdle())
    {
        apply_baud(g_next_baud);
        g_baud_tick = now;
        g_baud_phase = 3U;
    }
}
#endif

static void increment_counter(volatile uint16_t *counter)
{
    if (*counter != 0xFFFFU)
    {
        (*counter)++;
    }
}

static uint16_t ring_count(uint8_t head, uint8_t tail)
{
    uint16_t count;

    if (head >= tail)
    {
        count = (uint16_t)head - (uint16_t)tail;
    }
    else
    {
        count = HOST_LINK_UART_RX_CAPACITY - (uint16_t)tail + (uint16_t)head;
    }

    return count;
}

void HostLinkUart_Init(void)
{
#ifndef BRIDGE_SPLIT_BOOT
    g_baud = HL_BAUD_DEFAULT;
    g_next_baud = HL_BAUD_DEFAULT;
    g_baud_phase = 0U;
    g_rate_changed = 0U;
#endif
    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_tx_head = 0U;
    g_tx_tail = 0U;
    g_tx_busy = 0U;
    g_tx_active_length = 0U;
    g_uart_stats.rx_overflow_count = 0U;
    g_uart_stats.rx_framing_error_count = 0U;
    g_uart_stats.rx_parity_error_count = 0U;
    g_uart_stats.rx_hardware_overrun_count = 0U;
    g_uart_stats.tx_overflow_count = 0U;
    g_uart_stats.tx_start_error_count = 0U;
    g_uart_stats.rx_high_water = 0U;
    g_uart_stats.tx_high_water = 0U;
}

void HostLinkUart_Process(void)
{
    uint8_t head;
    uint8_t tail;
    uint16_t length;
    MD_STATUS status;

#ifndef BRIDGE_SPLIT_BOOT
    process_baud();
#endif

    if (g_tx_busy != 0U)
    {
        return;
    }

    head = g_tx_head;
    tail = g_tx_tail;
    if (head == tail)
    {
        return;
    }

    if (head > tail)
    {
        length = (uint16_t)head - (uint16_t)tail;
    }
    else
    {
        length = HOST_LINK_UART_TX_CAPACITY - (uint16_t)tail;
    }

    g_tx_active_length = length;
    g_tx_busy = 1U;
    status = R_Config_UART0_Send(&g_tx_buffer[tail], length);
    if (status != MD_OK)
    {
        g_tx_busy = 0U;
        g_tx_active_length = 0U;
        increment_counter(&g_uart_stats.tx_start_error_count);
    }
}

uint16_t HostLinkUart_Read(uint8_t *destination, uint16_t maximum_length)
{
    uint16_t count;
    uint8_t tail;

    count = 0U;
    if (destination == (uint8_t *)0)
    {
        return 0U;
    }

    tail = g_rx_tail;
    while ((count < maximum_length) && (tail != g_rx_head))
    {
        destination[count] = g_rx_buffer[tail];
        tail++;
        count++;
    }
    g_rx_tail = tail;

    return count;
}

uint8_t HostLinkUart_Write(const uint8_t *source, uint16_t length)
{
    uint16_t free_count;
    uint16_t index;
    uint8_t head;

    if ((source == (const uint8_t *)0) || (length == 0U))
    {
        return 0U;
    }

    free_count = HostLinkUart_GetTxFree();
    if (length > free_count)
    {
        increment_counter(&g_uart_stats.tx_overflow_count);
        return 0U;
    }

    head = g_tx_head;
    for (index = 0U; index < length; index++)
    {
        g_tx_buffer[head] = source[index];
        head++;
    }
    g_tx_head = head;
    free_count = ring_count(g_tx_head, g_tx_tail);
    if (free_count > g_uart_stats.tx_high_water)
    {
        g_uart_stats.tx_high_water = free_count;
    }

    return 1U;
}

uint16_t HostLinkUart_GetRxCount(void)
{
    return ring_count(g_rx_head, g_rx_tail);
}

uint16_t HostLinkUart_GetTxFree(void)
{
    uint16_t used;

    used = ring_count(g_tx_head, g_tx_tail);
    return (HOST_LINK_UART_TX_CAPACITY - 1U) - used;
}

uint8_t HostLinkUart_IsTxIdle(void)
{
    return ((g_tx_head == g_tx_tail) && (g_tx_busy == 0U)) ? 1U : 0U;
}

void HostLinkUart_GetStats(host_link_uart_stats_t *stats)
{
    if (stats == (host_link_uart_stats_t *)0)
    {
        return;
    }

    BSP_DI();
    stats->rx_overflow_count = g_uart_stats.rx_overflow_count;
    stats->rx_framing_error_count = g_uart_stats.rx_framing_error_count;
    stats->rx_parity_error_count = g_uart_stats.rx_parity_error_count;
    stats->rx_hardware_overrun_count = g_uart_stats.rx_hardware_overrun_count;
    stats->tx_overflow_count = g_uart_stats.tx_overflow_count;
    stats->tx_start_error_count = g_uart_stats.tx_start_error_count;
    stats->rx_high_water = g_uart_stats.rx_high_water;
    stats->tx_high_water = g_uart_stats.tx_high_water;
    BSP_EI();
}

void HostLinkUart_ClearStats(void)
{
    BSP_DI();
    g_uart_stats.rx_overflow_count = 0U;
    g_uart_stats.rx_framing_error_count = 0U;
    g_uart_stats.rx_parity_error_count = 0U;
    g_uart_stats.rx_hardware_overrun_count = 0U;
    g_uart_stats.tx_overflow_count = 0U;
    g_uart_stats.tx_start_error_count = 0U;
    g_uart_stats.rx_high_water = ring_count(g_rx_head, g_rx_tail);
    g_uart_stats.tx_high_water = ring_count(g_tx_head, g_tx_tail);
    BSP_EI();
}

void HostLinkUart_RxByteFromIsr(uint8_t data)
{
    uint8_t next;
    uint16_t used;

    next = (uint8_t)(g_rx_head + 1U);
    if (next == g_rx_tail)
    {
        increment_counter(&g_uart_stats.rx_overflow_count);
    }
    else
    {
        g_rx_buffer[g_rx_head] = data;
        g_rx_head = next;
        used = ring_count(g_rx_head, g_rx_tail);
        if (used > g_uart_stats.rx_high_water)
        {
            g_uart_stats.rx_high_water = used;
        }
    }
}

void HostLinkUart_RxErrorFromIsr(uint8_t error_flags)
{
    if ((error_flags & UART_ERROR_OVERRUN) != 0U)
    {
        increment_counter(&g_uart_stats.rx_hardware_overrun_count);
    }
    if ((error_flags & UART_ERROR_PARITY) != 0U)
    {
        increment_counter(&g_uart_stats.rx_parity_error_count);
    }
    if ((error_flags & UART_ERROR_FRAMING) != 0U)
    {
        increment_counter(&g_uart_stats.rx_framing_error_count);
    }
}

void HostLinkUart_TxCompleteFromIsr(void)
{
    g_tx_tail = (uint8_t)(g_tx_tail + (uint8_t)g_tx_active_length);
    g_tx_active_length = 0U;
    g_tx_busy = 0U;
}
