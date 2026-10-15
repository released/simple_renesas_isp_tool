#include <string.h>
#include "bridge_i2c.h"

#define I2C_STAGE_IDLE (0U)
#define I2C_STAGE_UPLOAD (1U)
#define I2C_STAGE_TX (2U)
#define I2C_STAGE_GAP (3U)
#define I2C_STAGE_RX (4U)
#define I2C_STAGE_COMPLETE (5U)
#define I2C_STAGE_EXPIRED (6U)

static uint8_t g_tx[BRIDGE_I2C_LIMIT];
static uint8_t g_rx[BRIDGE_I2C_LIMIT];
static uint16_t g_tx_length;
static uint16_t g_rx_length;
static uint16_t g_uploaded;
static uint16_t g_timeout;
static uint16_t g_gap;
static uint16_t g_started;
static uint16_t g_last;
static uint8_t g_address;
static uint8_t g_repeat;
static uint8_t g_stage;
static uint8_t g_configured;
static uint8_t g_port;
static uint8_t g_result;
static uint8_t g_result_ready;

static uint16_t I2cRead16(const uint8_t *data)
{
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint16_t I2cElapsed(uint16_t now, uint16_t before)
{
    return (now >= before) ? (uint16_t)(now - before) :
        (uint16_t)(60000U - before + now);
}

static void I2cFinish(uint8_t status)
{
    if (status != 0U)
    {
        Bridge_I2cBackend_Abort();
    }
    g_result = status;
    g_result_ready = 1U;
    g_stage = I2C_STAGE_COMPLETE;
}

void Bridge_I2cInit(void)
{
    g_stage = I2C_STAGE_IDLE;
    g_configured = 0U;
    g_result_ready = 0U;
}

uint8_t Bridge_I2cActive(void)
{
    return ((g_stage == I2C_STAGE_TX) || (g_stage == I2C_STAGE_GAP) ||
            (g_stage == I2C_STAGE_RX)) ? 1U : 0U;
}

uint8_t Bridge_I2cReserved(void)
{
    return ((g_stage == I2C_STAGE_UPLOAD) || Bridge_I2cActive()) ? 1U : 0U;
}

void Bridge_I2cAbort(void)
{
    Bridge_I2cBackend_Abort();
    g_stage = I2C_STAGE_IDLE;
    g_result_ready = 0U;
    g_uploaded = 0U;
}

uint8_t Bridge_I2cConfigure(uint8_t port, uint8_t speed)
{
    uint8_t status;

    if (Bridge_I2cReserved() != 0U)
    {
        return 1U;
    }
    g_configured = 0U;
    status = Bridge_I2cBackend_Open(port, speed);
    if (status == 0U)
    {
        g_configured = 1U;
        g_port = port;
        g_stage = I2C_STAGE_IDLE;
        g_result_ready = 0U;
    }
    return status;
}

uint8_t Bridge_I2cCommand(const uint8_t *input, uint16_t length,
                        uint8_t *output, uint16_t *output_length,
                        uint16_t now)
{
    uint16_t tx_length;
    uint16_t rx_length;
    uint16_t offset;
    uint16_t count;
    uint16_t timeout;
    uint16_t gap;
    uint8_t status;

    *output_length = 0U;
    if (length == 0U)
    {
        return 3U;
    }
    if (input[0] == 4U)
    {
        if (length != 1U) return 3U;
        Bridge_I2cAbort();
        return 0U;
    }
    if (Bridge_I2cActive() != 0U) return 1U;
    if (g_configured == 0U) return 10U;
    if (input[0] == 0U)
    {
        if (length != 12U) return 3U;
        tx_length = I2cRead16(&input[4]);
        rx_length = I2cRead16(&input[6]);
        timeout = I2cRead16(&input[8]);
        gap = I2cRead16(&input[10]);
        if ((input[1] != g_port) || (input[2] < 8U) || (input[2] > 0x77U) ||
            (input[3] > 1U) || (tx_length > BRIDGE_I2C_LIMIT) ||
            (rx_length > BRIDGE_I2C_LIMIT) ||
            ((tx_length == 0U) && (rx_length == 0U)) ||
            (timeout == 0U) || (timeout > 30000U) || (gap > timeout) ||
            ((input[3] != 0U) &&
             ((tx_length == 0U) || (rx_length == 0U) || (gap != 0U))))
        {
            return 3U;
        }
        if (g_stage == I2C_STAGE_UPLOAD) return 1U;
        g_tx_length = tx_length;
        g_rx_length = rx_length;
        g_address = input[2];
        g_repeat = input[3];
        g_timeout = timeout;
        g_gap = gap;
        g_uploaded = 0U;
        g_result_ready = 0U;
        g_result = 0U;
        g_stage = I2C_STAGE_UPLOAD;
        g_last = now;
        return 0U;
    }
    if (input[0] == 1U)
    {
        if ((length < 4U) || (length > HOST_WIRE_MAX_PAYLOAD)) return 3U;
        if (g_stage != I2C_STAGE_UPLOAD) return 10U;
        offset = I2cRead16(&input[1]);
        count = (uint16_t)(length - 3U);
        if ((offset != g_uploaded) || (offset > g_tx_length) ||
            (count > (uint16_t)(g_tx_length - offset))) return 3U;
        memcpy(&g_tx[offset], &input[3], count);
        g_uploaded = (uint16_t)(g_uploaded + count);
        g_last = now;
        return 0U;
    }
    if (input[0] == 2U)
    {
        if (length != 1U) return 3U;
        if ((g_stage != I2C_STAGE_UPLOAD) ||
            (g_uploaded != g_tx_length)) return 10U;
        g_started = now;
        g_last = now;
        if (g_tx_length != 0U)
        {
            status = Bridge_I2cBackend_Write(g_address, g_tx, g_tx_length,
                                            g_repeat);
            g_stage = I2C_STAGE_TX;
        }
        else
        {
            status = Bridge_I2cBackend_Read(g_address, g_rx, g_rx_length);
            g_stage = I2C_STAGE_RX;
        }
        if (status != 0U) I2cFinish(status);
        return BRIDGE_I2C_PENDING;
    }
    if (input[0] == 3U)
    {
        if (length != 4U) return 3U;
        if ((g_stage != I2C_STAGE_COMPLETE) || (g_result != 0U)) return 10U;
        offset = I2cRead16(&input[1]);
        count = input[3];
        if ((count == 0U) || (count > BRIDGE_I2C_CHUNK_LIMIT) || (offset > g_rx_length) ||
            (count > (uint16_t)(g_rx_length - offset))) return 3U;
        output[0] = (uint8_t)offset;
        output[1] = (uint8_t)(offset >> 8);
        memcpy(&output[2], &g_rx[offset], count);
        *output_length = (uint16_t)(count + 2U);
        return 0U;
    }
    return 3U;
}

void Bridge_I2cPoll(uint16_t now)
{
    uint8_t status;

    if ((g_stage == I2C_STAGE_UPLOAD) &&
        (I2cElapsed(now, g_last) >= 5000U))
    {
        g_stage = I2C_STAGE_EXPIRED;
        return;
    }
    if (Bridge_I2cActive() == 0U) return;
    if (I2cElapsed(now, g_started) >= g_timeout)
    {
        I2cFinish(6U);
        return;
    }
    if (g_stage == I2C_STAGE_GAP)
    {
        if (I2cElapsed(now, g_last) < g_gap) return;
        status = Bridge_I2cBackend_Read(g_address, g_rx, g_rx_length);
        g_stage = I2C_STAGE_RX;
        if (status != 0U) I2cFinish(status);
        return;
    }
    status = Bridge_I2cBackend_Poll();
    if (status == BRIDGE_I2C_PENDING) return;
    if (status != 0U)
    {
        I2cFinish(status);
        return;
    }
    if ((g_stage == I2C_STAGE_TX) && (g_rx_length != 0U))
    {
        g_last = now;
        g_stage = I2C_STAGE_GAP;
    }
    else
    {
        I2cFinish(0U);
    }
}

uint8_t Bridge_I2cTakeResult(uint8_t *status, uint16_t *received)
{
    if (g_result_ready == 0U) return 0U;
    *status = g_result;
    *received = (g_result == 0U) ? g_rx_length : 0U;
    g_result_ready = 0U;
    return 1U;
}
