#ifndef DEVICE_UART_RATES_H
#define DEVICE_UART_RATES_H
/* GET_PORT_INFO bus2: bit0 preserves115200 support;bit31 permits numeric
 * Device baud requests in this range. SET_UART_CONFIG must still ACK the
 * requested nominal rate;the backend may reject an inaccurate divisor.
 * This contract is independent of PC Host Link UART baud negotiation.
 */
#define DEVICE_UART_RATE_DEFAULT (0x00000001UL)
#define DEVICE_UART_RATE_NUMERIC (0x80000000UL)
#define DEVICE_UART_RATE_MASK (0x80000001UL)
#define DEVICE_UART_BAUD_MIN (300UL)
#define DEVICE_UART_BAUD_MAX (2000000UL)
#endif
