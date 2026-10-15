#ifndef HOST_LINK_UART_H
#define HOST_LINK_UART_H

#include <stdint.h>
#include "../common/host_link_limits.h"
#include "../common/host_link_baud.h"

#define HOST_LINK_UART_RX_CAPACITY    (256U)
#define HOST_LINK_UART_TX_CAPACITY    (256U)
#if HOST_WIRE_MAX_FRAME > (HOST_LINK_UART_TX_CAPACITY - 1U)
#error Complete Host Link frame must fit in the UART TX ring.
#endif
#if HOST_LINK_UART_RX_CAPACITY != 256U || HOST_LINK_UART_TX_CAPACITY != 256U
#error UART8-bit ring indices require256-byte arrays; audit ISR atomicity before resizing.
#endif

typedef struct
{
    uint16_t rx_overflow_count;
    uint16_t rx_framing_error_count;
    uint16_t rx_parity_error_count;
    uint16_t rx_hardware_overrun_count;
    uint16_t tx_overflow_count;
    uint16_t tx_start_error_count;
} host_link_uart_stats_t;

void HostLinkUart_Init(void);
void HostLinkUart_Process(void);
void HostLinkUart_BaudInfo(uint32_t *nominal, uint32_t *actual);
uint8_t HostLinkUart_PrepareBaud(uint32_t baud);
void HostLinkUart_ArmBaud(void);
uint8_t HostLinkUart_ConfirmBaud(uint32_t baud);
uint8_t HostLinkUart_BaudPending(void);
uint8_t HostLinkUart_TakeRateChange(void);

uint16_t HostLinkUart_Read(uint8_t *destination, uint16_t maximum_length);
uint8_t HostLinkUart_Write(const uint8_t *source, uint16_t length);
uint16_t HostLinkUart_GetRxCount(void);
uint16_t HostLinkUart_GetTxFree(void);
uint8_t HostLinkUart_IsTxIdle(void);
void HostLinkUart_GetStats(host_link_uart_stats_t *stats);
void HostLinkUart_ClearStats(void);

void HostLinkUart_RxByteFromIsr(uint8_t data);
void HostLinkUart_RxErrorFromIsr(uint8_t error_flags);
void HostLinkUart_TxCompleteFromIsr(void);

#endif
