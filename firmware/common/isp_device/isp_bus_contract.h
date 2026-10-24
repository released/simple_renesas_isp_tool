#ifndef ISP_BUS_CONTRACT_H
#define ISP_BUS_CONTRACT_H
/* Independent literal C inputs are also consumed by the PowerShell image
 * reader. Reject disagreement instead of advertising an unrelated link rate.
 */
#if defined(ISP_I2C)
#if ISP_BUS_RATE != 100000UL && ISP_BUS_RATE != 400000UL
#error I2C device rate must match the supported100/400kHz contract.
#endif
#if ISP_I2C_ADDRESS < 0x08U || ISP_I2C_ADDRESS > 0x77U
#error Use a non-reserved seven-bit I2C address.
#endif
#if ISP_LINK_PARAMETER_0 != ISP_BUS_RATE || ISP_LINK_PARAMETER_1 != ISP_I2C_ADDRESS
#error I2C metadata rate/address must match the board contract.
#endif
#if ISP_POLICY_UART_BAUD != 0UL
#error I2C policy does not advertise a device UART baud.
#endif
#else
#if defined(ISP_UART_BAUD)
#if ISP_UART_BAUD < 300UL || ISP_UART_BAUD > 2000000UL
#error Device UART baud must be300..2000000.
#endif
#if defined(ISP_BUS_RATE) && ISP_BUS_RATE != ISP_UART_BAUD
#error UART INFO rate differs from ISP_UART_BAUD.
#endif
#if defined(ISP_POLICY_UART_BAUD) && ISP_POLICY_UART_BAUD != ISP_UART_BAUD
#error UART metadata rate differs from ISP_UART_BAUD.
#endif
#else
#if ISP_BUS_RATE < 300UL || ISP_BUS_RATE > 2000000UL || ISP_POLICY_UART_BAUD != ISP_BUS_RATE
#error UART INFO and metadata must declare the same valid device baud.
#endif
#endif
#endif
#endif
