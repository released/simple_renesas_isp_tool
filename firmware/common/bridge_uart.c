#include <string.h>
#include "bridge_uart.h"

#define UART_IDLE (0U)
#define UART_UPLOAD (1U)
#define UART_RUNNING (2U)
#define UART_COMPLETE (3U)

static uint8_t g_tx[BRIDGE_UART_LIMIT], g_rx[BRIDGE_UART_LIMIT];
static uint16_t g_tx_length, g_rx_max, g_rx_min, g_uploaded, g_received;
static uint16_t g_timeout, g_idle_ms, g_started, g_last;
static uint8_t g_stage, g_configured, g_port, g_clear_rx;
static uint8_t g_result, g_result_ready;

static uint16_t UartRead16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint16_t UartElapsed(uint16_t now, uint16_t before)
{
    return (now >= before) ? (uint16_t)(now - before) : (uint16_t)(60000U - before + now);
}
static void UartFinish(uint8_t result, uint16_t now)
{
    if (result != 0U)
    {
        Bridge_UartBackend_Abort();
    }
    g_result = result;
    g_result_ready = 1U;
    g_stage = UART_COMPLETE;
    g_last = now;
}
void Bridge_UartInit(void)
{
    g_stage = UART_IDLE;
    g_configured = 0U;
    g_result_ready = 0U;
}
uint8_t Bridge_UartActive(void)
{
    return (g_stage == UART_RUNNING) ? 1U : 0U;
}
uint8_t Bridge_UartReserved(void)
{
    return (g_stage != UART_IDLE) ? 1U : 0U;
}
void Bridge_UartAbort(void)
{
    Bridge_UartBackend_Abort();
    g_stage = UART_IDLE;
    g_result_ready = 0U;
}
uint8_t Bridge_UartConfigure(uint8_t port, uint32_t baud)
{
    uint8_t result;
    if (Bridge_UartReserved())
    {
        return 1U;
    }
    g_configured = 0U;
    result = Bridge_UartBackend_Open(port, baud);
    if (result == 0U)
    {
        g_configured = 1U;
        g_port = port;
    }
    return result;
}
uint8_t Bridge_UartCommand(const uint8_t *input, uint16_t length, uint8_t *output,
                           uint16_t *output_length, uint16_t now)
{
    uint16_t tx_length, rx_max, rx_min, timeout, idle_ms, offset, count;
    uint8_t result;
    *output_length = 0U;
    if (length == 0U)
    {
        return 3U;
    }
    if (input[0] == 4U)
    {
        if (length != 1U)
        {
            return 3U;
        }
        Bridge_UartAbort();
        return 0U;
    }
    if (Bridge_UartActive())
    {
        return 1U;
    }
    if (!g_configured)
    {
        return 10U;
    }
    if (input[0] == 0U)
    {
        if (length != 14U)
        {
            return 3U;
        }
        tx_length = UartRead16(input + 4U);
        rx_max = UartRead16(input + 6U);
        rx_min = UartRead16(input + 8U);
        timeout = UartRead16(input + 10U);
        idle_ms = UartRead16(input + 12U);
        if (input[1] != g_port || input[2] > 1U || input[3] != 0U ||
            tx_length > BRIDGE_UART_LIMIT || rx_max > BRIDGE_UART_LIMIT || rx_min > rx_max ||
            (!tx_length && !rx_max) || (rx_max && !rx_min) || !timeout || timeout > 30000U ||
            (rx_min < rx_max && !idle_ms) || idle_ms > timeout)
        {
            return 3U;
        }
        if (Bridge_UartReserved())
        {
            return 1U;
        }
        g_tx_length = tx_length;
        g_rx_max = rx_max;
        g_rx_min = rx_min;
        g_timeout = timeout;
        g_idle_ms = idle_ms;
        g_clear_rx = input[2];
        g_uploaded = 0U;
        g_received = 0U;
        g_result_ready = 0U;
        g_stage = UART_UPLOAD;
        g_last = now;
        return 0U;
    }
    if (input[0] == 1U)
    {
        if (length < 4U || length > HOST_WIRE_MAX_PAYLOAD)
        {
            return 3U;
        }
        if (g_stage != UART_UPLOAD)
        {
            return 10U;
        }
        offset = UartRead16(input + 1U);
        count = (uint16_t)(length - 3U);
        if (offset != g_uploaded || offset > g_tx_length ||
            count > (uint16_t)(g_tx_length - offset))
        {
            return 3U;
        }
        memcpy(g_tx + offset, input + 3U, count);
        g_uploaded = (uint16_t)(g_uploaded + count);
        g_last = now;
        return 0U;
    }
    if (input[0] == 2U)
    {
        if (length != 1U)
        {
            return 3U;
        }
        if (g_stage != UART_UPLOAD || g_uploaded != g_tx_length)
        {
            return 10U;
        }
        g_started = now;
        g_last = now;
        g_stage = UART_RUNNING;
        result = Bridge_UartBackend_Begin(g_tx, g_tx_length, g_clear_rx);
        if (result != 0U)
        {
            UartFinish(result, now);
        }
        return BRIDGE_UART_PENDING;
    }
    if (input[0] == 3U)
    {
        if (length != 4U)
        {
            return 3U;
        }
        if (g_stage != UART_COMPLETE || g_result != 0U)
        {
            return 10U;
        }
        offset = UartRead16(input + 1U);
        count = input[3];
        if (!count || count > HOST_WIRE_MAX_PAYLOAD - 3U || offset > g_received ||
            count > (uint16_t)(g_received - offset))
        {
            return 3U;
        }
        output[0] = (uint8_t)offset;
        output[1] = (uint8_t)(offset >> 8);
        memcpy(output + 2U, g_rx + offset, count);
        *output_length = (uint16_t)(count + 2U);
        g_last = now;
        return 0U;
    }
    if (input[0] == 5U)
    {
        if (length != 1U)
        {
            return 3U;
        }
        if (g_stage != UART_COMPLETE || g_result != 0U || g_result_ready)
        {
            return 10U;
        }
        /* Release staging without discarding asynchronous UART receive bytes. */
        g_stage = UART_IDLE;
        return 0U;
    }
    return 3U;
}
void Bridge_UartPoll(uint16_t now)
{
    uint8_t value, error, budget;
    if (g_stage == UART_UPLOAD || g_stage == UART_COMPLETE)
    {
        /* Never discard a result before the outer dispatcher can queue it. */
        if (!g_result_ready && UartElapsed(now, g_last) >= 5000U)
        {
            Bridge_UartAbort();
        }
        return;
    }
    if (!Bridge_UartActive())
    {
        return;
    }
    error = Bridge_UartBackend_Error();
    if (error)
    {
        UartFinish(error, now);
        return;
    }
    for (budget = 0U; g_rx_max != 0U && budget < 64U; ++budget)
    {
        if (!Bridge_UartBackend_Get(&value))
        {
            break;
        }
        if (g_received == g_rx_max)
        {
            UartFinish(8U, now);
            return;
        }
        g_rx[g_received++] = value;
        g_last = now;
    }
    error = Bridge_UartBackend_Error();
    if (error)
    {
        UartFinish(error, now);
        return;
    }
    if (Bridge_UartBackend_TxIdle())
    {
        if (g_received == g_rx_max ||
            (g_received >= g_rx_min && g_idle_ms && UartElapsed(now, g_last) >= g_idle_ms))
        {
            UartFinish(0U, now);
            return;
        }
    }
    if (UartElapsed(now, g_started) >= g_timeout)
    {
        UartFinish(6U, now);
    }
}
uint8_t Bridge_UartTakeResult(uint8_t *status, uint16_t *received)
{
    if (!g_result_ready)
    {
        return 0U;
    }
    *status = g_result;
    *received = g_received;
    g_result_ready = 0U;
    return 1U;
}
