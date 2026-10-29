#ifndef BRIDGE_I2C_H
#define BRIDGE_I2C_H

#include <stdint.h>
#include "host_link_limits.h"

#define BRIDGE_I2C_LIMIT (512U)
#define BRIDGE_I2C_CHUNK_LIMIT (HOST_WIRE_MAX_PAYLOAD - 3U)
#define BRIDGE_I2C_PENDING (0xFFU)
#define BRIDGE_I2C_NACK (12U)
#define BRIDGE_I2C_ARBITRATION (13U)
#define BRIDGE_I2C_BUS_ERROR (14U)

/* These APIs are identical on F24/RA hosts; only the backend owns registers.
 * Addresses are unshifted 7-bit values. UART staging never splits I2C packets.
 * Poll and IRQ work are bounded; all target flash semantics stay on the PC. */
void Bridge_I2cInit(void);
uint8_t Bridge_I2cConfigure(uint8_t port, uint8_t speed);
uint8_t Bridge_I2cCommand(const uint8_t *input, uint16_t length, uint8_t *output,
                          uint16_t *output_length, uint16_t now);
void Bridge_I2cPoll(uint16_t now);
uint8_t Bridge_I2cActive(void);
uint8_t Bridge_I2cReserved(void);
uint8_t Bridge_I2cTakeResult(uint8_t *status, uint16_t *received);
void Bridge_I2cAbort(void);

uint8_t Bridge_I2cBackend_Open(uint8_t port, uint8_t speed);
uint8_t Bridge_I2cBackend_Write(uint8_t address, const uint8_t *data, uint16_t length,
                                uint8_t hold_bus);
uint8_t Bridge_I2cBackend_Read(uint8_t address, uint8_t *data, uint16_t length);
uint8_t Bridge_I2cBackend_Poll(void);
void Bridge_I2cBackend_Abort(void);
/* Returns one when the custom backend owns this interrupt. */
uint8_t Bridge_I2cBackend_Irq(void);

#endif
