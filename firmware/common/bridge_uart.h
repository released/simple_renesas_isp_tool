#ifndef BRIDGE_UART_H
#define BRIDGE_UART_H

#include <stdint.h>
#include "host_link_limits.h"
#include "device_uart_rates.h"

#define BRIDGE_UART_FEATURE (0x00004000UL)
#define BRIDGE_UART_CONFIG_COMMAND (0x60U)
#define BRIDGE_UART_TRANSACTION_COMMAND (0x61U)
#define BRIDGE_UART_STATS_COMMAND (0x62U)
#define BRIDGE_UART_LIMIT (512U)
#define BRIDGE_UART_PENDING (0xFFU)
#define BRIDGE_UART_BAUD (115200UL)

/* Generic bytes only: BEGIN/CHUNK never send; EXEC sends once. Buffers remain
 * reserved through FETCH until ABORT or the five-second idle lease expires.
 * All deadlines use the host's 60000-ms tick, not a uint16 rollover. */
void Bridge_UartInit(void);
uint8_t Bridge_UartConfigure(uint8_t port, uint32_t baud);
uint8_t Bridge_UartCommand(const uint8_t *input, uint16_t length,
                          uint8_t *output, uint16_t *output_length,
                          uint16_t now);
void Bridge_UartPoll(uint16_t now);
uint8_t Bridge_UartReserved(void);
uint8_t Bridge_UartActive(void);
uint8_t Bridge_UartTakeResult(uint8_t *status, uint16_t *received);
void Bridge_UartAbort(void);

/* Backend IRQs only move bytes and latch errors; main owns protocol/timeouts.
 * TX storage remains valid until TxIdle or Abort. Error is latched per EXEC.
 * Begin explicitly clears stale RX only when flags bit0 requests it. */
uint8_t Bridge_UartBackend_Open(uint8_t port, uint32_t baud);
uint8_t Bridge_UartBackend_Begin(const uint8_t *tx, uint16_t length,
                                uint8_t clear_rx);
uint8_t Bridge_UartBackend_Get(uint8_t *value);
uint8_t Bridge_UartBackend_TxIdle(void);
uint8_t Bridge_UartBackend_Error(void);
void Bridge_UartBackend_Abort(void);
/* Six LE32 lifetime counters: RX, TX, RX errors, RX drops, RX peak, aborts.
 * Counters saturate, survive transactions and are not hardware acceptance. */
void Bridge_UartBackend_Stats(uint8_t *output);

#endif
