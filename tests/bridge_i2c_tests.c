#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../firmware/common/bridge_i2c.h"

static uint8_t mock_result;
static uint16_t mock_tx_length;
static uint8_t mock_tx[512];
static uint8_t mock_hold;
static unsigned int mock_writes;
static unsigned int mock_reads;
static unsigned int mock_aborts;
static unsigned int failures;
#define VERIFY(c) do { if (!(c)) { failures++; printf("I2C engine FAIL line %u\n", (unsigned int)__LINE__); } } while (0)

uint8_t Bridge_I2cBackend_Open(uint8_t port, uint8_t speed)
{ return ((port == 0U) && (speed == 0U)) ? 0U : 3U; }
uint8_t Bridge_I2cBackend_Write(uint8_t address, const uint8_t *data,
                              uint16_t length, uint8_t hold)
{
    VERIFY(address == 9U);
    mock_writes++; mock_tx_length = length; mock_hold = hold;
    memcpy(mock_tx, data, length);
    return 0U;
}
uint8_t Bridge_I2cBackend_Read(uint8_t address, uint8_t *data, uint16_t length)
{
    uint16_t i;
    VERIFY(address == 9U); mock_reads++;
    for (i = 0U; i < length; i++) data[i] = (uint8_t)i;
    return 0U;
}
uint8_t Bridge_I2cBackend_Poll(void) { return mock_result; }
void Bridge_I2cBackend_Abort(void) { mock_aborts++; }
uint8_t Bridge_I2cBackend_Irq(void) { return 0U; }

static uint8_t Command(uint8_t *input, uint16_t length, uint16_t now)
{
    uint8_t output[HOST_WIRE_MAX_PAYLOAD]; uint16_t count;
    return Bridge_I2cCommand(input, length, output, &count, now);
}
static void Begin(uint16_t tx, uint16_t rx, uint8_t repeat, uint16_t gap,
                  uint16_t now)
{
    uint8_t command[12];
    memset(command, 0, sizeof(command));
    command[2] = 9U; command[3] = repeat;
    command[4] = (uint8_t)tx; command[5] = (uint8_t)(tx >> 8);
    command[6] = (uint8_t)rx; command[7] = (uint8_t)(rx >> 8);
    command[8] = 100U;
    command[10] = (uint8_t)gap; command[11] = (uint8_t)(gap >> 8);
    VERIFY(Command(command, 12U, now) == 0U);
}
static void Upload(uint16_t length, uint16_t now)
{
    uint8_t chunk[HOST_WIRE_MAX_PAYLOAD]; uint16_t offset; uint16_t count; uint16_t i;
    offset = 0U;
    while (offset < length)
    {
        count = (length - offset > BRIDGE_I2C_CHUNK_LIMIT) ? BRIDGE_I2C_CHUNK_LIMIT : (uint16_t)(length - offset);
        chunk[0] = 1U; chunk[1] = (uint8_t)offset; chunk[2] = (uint8_t)(offset >> 8);
        for (i = 0U; i < count; i++) chunk[i + 3U] = (uint8_t)(offset + i);
        VERIFY(Command(chunk, (uint16_t)(count + 3U), now) == 0U);
        offset = (uint16_t)(offset + count);
    }
}
int TestBridgeI2c(void)
{
    uint8_t execute[1]; uint8_t status; uint16_t received;
    uint8_t fetch[4]; uint8_t output[HOST_WIRE_MAX_PAYLOAD]; uint16_t output_length;
    uint8_t bad[12]; unsigned int before; uint16_t i;

    failures = 0U; execute[0] = 2U; mock_result = 0U;
    Bridge_I2cInit();
    VERIFY(Command(execute, 1U, 0U) == 10U);
    VERIFY(Bridge_I2cConfigure(0U, 1U) == 3U);
    VERIFY(Bridge_I2cConfigure(0U, 0U) == 0U);
    Begin(262U, 512U, 1U, 0U, 0U); Upload(262U, 1U);
    VERIFY(mock_writes == 0U && mock_reads == 0U);
    VERIFY(Command(execute, 1U, 2U) == BRIDGE_I2C_PENDING);
    VERIFY(mock_writes == 1U && mock_tx_length == 262U && mock_hold == 1U);
    for (i = 0U; i < 262U; i++) VERIFY(mock_tx[i] == (uint8_t)i);
    VERIFY(Command(execute, 1U, 2U) == 1U);
    Bridge_I2cPoll(3U); Bridge_I2cPoll(4U); Bridge_I2cPoll(5U);
    VERIFY(Bridge_I2cTakeResult(&status, &received) == 1U);
    VERIFY(status == 0U && received == 512U && mock_reads == 1U);
    VERIFY(Bridge_I2cTakeResult(&status, &received) == 0U);
    fetch[0] = 3U; fetch[1] = 255U; fetch[2] = 1U; fetch[3] = 1U;
    VERIFY(Bridge_I2cCommand(fetch, 4U, output, &output_length, 6U) == 0U);
    VERIFY(output_length == 3U && output[2] == 255U);
    fetch[1] = 0U; fetch[2] = 2U;
    VERIFY(Bridge_I2cCommand(fetch, 4U, output, &output_length, 6U) == 3U);

    memset(bad, 0, sizeof(bad)); bad[2] = 9U; bad[4] = 1U; bad[8] = 100U;
    bad[5] = 3U; VERIFY(Command(bad, 12U, 0U) == 3U); bad[5] = 0U;
    bad[2] = 0U; VERIFY(Command(bad, 12U, 0U) == 3U); bad[2] = 9U;
    bad[3] = 1U; VERIFY(Command(bad, 12U, 0U) == 3U); bad[3] = 0U;
    before = mock_aborts;
    Begin(1U, 1U, 0U, 20U, 59990U); Upload(1U, 59990U);
    VERIFY(Command(execute, 1U, 59990U) == BRIDGE_I2C_PENDING);
    mock_result = BRIDGE_I2C_PENDING;
    Bridge_I2cPoll(90U);
    VERIFY(Bridge_I2cTakeResult(&status, &received) == 1U && status == 6U);
    VERIFY(mock_aborts == before + 1U);
    for (status = BRIDGE_I2C_NACK; status <= BRIDGE_I2C_BUS_ERROR; status++)
    {
        mock_result = status; Begin(1U, 0U, 0U, 0U, 0U); Upload(1U, 0U);
        VERIFY(Command(execute, 1U, 0U) == BRIDGE_I2C_PENDING);
        Bridge_I2cPoll(1U);
        VERIFY(Bridge_I2cTakeResult(&bad[0], &received) == 1U && bad[0] == status);
    }
    mock_result = 0U;
    Begin(1U, 0U, 0U, 0U, 0U); Bridge_I2cPoll(5000U);
    VERIFY(Bridge_I2cReserved() == 0U && Command(execute, 1U, 5000U) == 10U);
    Begin(1U, 0U, 0U, 0U, 0U);
    VERIFY(Command(execute, 1U, 0U) == 10U); /* Incomplete staging cannot TX. */
    Bridge_I2cAbort(); Bridge_I2cAbort();
    VERIFY(Bridge_I2cReserved() == 0U);
    if (failures == 0U) printf("I2C real-source staging,262-byte continuity,bounds,error and timeout tests passed\n");
    return failures == 0U;
}
