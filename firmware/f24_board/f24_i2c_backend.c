#ifdef BRIDGE_I2C_BACKEND_TEST
#include "f24_i2c_mock.h"
#else
#include "r_cg_macrodriver.h"
#include "Config_IICA0.h"
#include "f24_can_clock.h"
#endif
#include "bridge_i2c.h"

/* Generated callbacks always emit STOP and cannot express repeated-start.
 * A preserved INTIICA0 hook routes owned transfers here instead. The bounded
 * IRQ handler only moves bytes and records state; protocol work stays in main.
 * IICA0 CPU40MHz/fMCK20MHz,SCL=P62,SDA=P63. R01UH0944 section16.4.2:
 * fast-mode low/high=0.52/0.48 of the requested cycle. Conservative26/24
 * counts at400K assume zero edge time;real edges can only lower SCL rate.
 * Keep generated100K timing94/106 unchanged. */
#define BACK_IDLE (0U)
#define BACK_START (1U)
#define BACK_ADDRESS (2U)
#define BACK_TX (3U)
#define BACK_RX (4U)
#define BACK_RX_END (5U)
#define BACK_HELD (6U)
#define BACK_STOP (7U)
#define BACK_ERROR (8U)

static volatile uint8_t g_backend_state;
static volatile uint8_t g_backend_error;
static volatile uint8_t g_backend_owned;
static uint8_t g_backend_read;
static uint8_t g_backend_address;
static uint8_t g_backend_hold;
static uint8_t g_backend_speed;
static uint16_t g_backend_length;
static volatile uint16_t g_backend_count;
static const uint8_t *g_backend_tx;
static uint8_t *g_backend_rx;

static void BackendCreate(void)
{
    R_Config_IICA0_Create();
    IICAMK0 = 1U; IICE0 = 0U;
    PRS0 = 1U;
    SMC0 = g_backend_speed;
    IICWL0 = g_backend_speed ? 26U : 94U;
    IICWH0 = g_backend_speed ? 24U : 106U;
    IICE0 = 1U;
}
uint8_t Bridge_I2cBackend_Open(uint8_t port, uint8_t speed)
{
    if ((port != 0U) || (speed > 1U)) return 3U;
    if (F24_CanClockIsReady() == 0U) return BRIDGE_I2C_BUS_ERROR;
    g_backend_owned = 1U;
    g_backend_state = BACK_IDLE;
    g_backend_error = 0U;
    g_backend_speed = speed;
    BackendCreate();
    return 0U;
}

static uint8_t BackendStart(uint8_t address, uint16_t length, uint8_t read)
{
    if ((length == 0U) || (length > BRIDGE_I2C_LIMIT) ||
        (address < 8U) || (address > 0x77U)) return 3U;
    if ((g_backend_state != BACK_IDLE) &&
        (g_backend_state != BACK_HELD)) return 1U;
    if (IICE0 == 0U)
    {
        BackendCreate();
    }
    IICAMK0 = 1U;
    if ((IICBSY0 != 0U) && (MSTS0 == 0U)) return 1U;
    g_backend_address = (uint8_t)((address << 1) | read);
    g_backend_length = length;
    g_backend_count = 0U;
    g_backend_read = read;
    g_backend_error = 0U;
    WTIM0 = 1U;
    ACKE0 = 1U;
    IICAIF0 = 0U;
    g_backend_state = BACK_START;
    STT0 = 1U;
    return 0U;
}

uint8_t Bridge_I2cBackend_Write(uint8_t address, const uint8_t *data,
                              uint16_t length, uint8_t hold_bus)
{
    g_backend_tx = data;
    g_backend_hold = hold_bus;
    return BackendStart(address, length, 0U);
}

uint8_t Bridge_I2cBackend_Read(uint8_t address, uint8_t *data, uint16_t length)
{
    g_backend_rx = data;
    g_backend_hold = 0U;
    return BackendStart(address, length, 1U);
}

static void BackendError(uint8_t status)
{
    g_backend_error = status;
    g_backend_state = BACK_ERROR;
    IICAMK0 = 1U;
}

uint8_t Bridge_I2cBackend_Irq(void)
{
    if (g_backend_owned == 0U) return 0U;
    if ((g_backend_state == BACK_IDLE) ||
        (g_backend_state == BACK_START)) return 1U;
    if (MSTS0 == 0U)
    {
        BackendError(BRIDGE_I2C_ARBITRATION);
        return 1U;
    }
    if (g_backend_state == BACK_ADDRESS)
    {
        if (ACKD0 == 0U)
        {
            BackendError(BRIDGE_I2C_NACK);
        }
        else if (g_backend_read != 0U)
        {
            g_backend_state = BACK_RX;
            ACKE0 = 1U;
            WTIM0 = 0U;
            WREL0 = 1U;
        }
        else
        {
            g_backend_state = BACK_TX;
            IICA0 = g_backend_tx[g_backend_count++];
        }
    }
    else if (g_backend_state == BACK_TX)
    {
        if (ACKD0 == 0U)
        {
            BackendError(BRIDGE_I2C_NACK);
        }
        else if (g_backend_count < g_backend_length)
        {
            IICA0 = g_backend_tx[g_backend_count++];
        }
        else if (g_backend_hold != 0U)
        {
            g_backend_state = BACK_HELD;
            IICAMK0 = 1U;
        }
        else
        {
            g_backend_state = BACK_STOP;
            SPT0 = 1U;
        }
    }
    else if (g_backend_state == BACK_RX)
    {
        if (g_backend_count >= g_backend_length)
        {
            BackendError(BRIDGE_I2C_BUS_ERROR);
            return 1U;
        }
        g_backend_rx[g_backend_count++] = IICA0;
        if (g_backend_count == g_backend_length)
        {
            ACKE0 = 0U;
            WTIM0 = 1U;
            g_backend_state = BACK_RX_END;
        }
        WREL0 = 1U;
    }
    else if (g_backend_state == BACK_RX_END)
    {
        g_backend_state = BACK_STOP;
        SPT0 = 1U;
    }
    return 1U;
}

uint8_t Bridge_I2cBackend_Poll(void)
{
    if (g_backend_state == BACK_START)
    {
        if (STD0 != 0U)
        {
            /* Do not enable the IRQ before the address/buffer state is ready. */
            IICAIF0 = 0U;
            g_backend_state = BACK_ADDRESS;
            IICA0 = g_backend_address;
            IICAMK0 = 0U;
        }
    }
    else if (g_backend_state == BACK_STOP)
    {
        if (IICBSY0 == 0U)
        {
            IICAMK0 = 1U;
            g_backend_state = BACK_IDLE;
            return 0U;
        }
    }
    else if (g_backend_state == BACK_HELD)
    {
        return 0U;
    }
    else if (g_backend_state == BACK_ERROR)
    {
        return g_backend_error;
    }
    return BRIDGE_I2C_PENDING;
}

void Bridge_I2cBackend_Abort(void)
{
    if (g_backend_owned == 0U) return;
    IICAMK0 = 1U;
    SPT0 = 1U;
    IICE0 = 0U;
    IICAIF0 = 0U;
    PM6 |= 0x0CU; /* Release open-drain lines; never force SDA/SCL HIGH. */
    g_backend_state = BACK_IDLE;
}
