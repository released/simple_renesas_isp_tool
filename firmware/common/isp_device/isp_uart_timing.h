#ifndef ISP_UART_TIMING_H
#define ISP_UART_TIMING_H

/* SAU asynchronous UART, fCK source selected by the board SPS register.
 * Supply the actual selected source clock, not CPU clock. SDR bits15..9
 * encode divisor minus one; UART0 logging retains its independent setting.
 * Example: 16 MHz / (2 * 69) = 115942 baud for requested115200 (0.65%).
 * This is a compile-time consistency check, not electrical qualification.
 */
#if !defined(ISP_UART_CLOCK_HZ) || !defined(ISP_UART_BAUD)
#error Define the board UART source clock and requested device baud.
#endif
#if ISP_UART_BAUD < 300UL || ISP_UART_BAUD > 2000000UL
#error Device UART baud must be 300..2000000.
#endif
#define ISP_UART_DIVISOR ((ISP_UART_CLOCK_HZ + ISP_UART_BAUD) / (2UL * ISP_UART_BAUD))
#if ISP_UART_DIVISOR < 1UL || ISP_UART_DIVISOR > 128UL
#error UART baud cannot fit this SAU source clock. Review SPS/source clock.
#endif
#define ISP_UART_ACTUAL_BAUD (ISP_UART_CLOCK_HZ / (2UL * ISP_UART_DIVISOR))
#if ISP_UART_ACTUAL_BAUD > ISP_UART_BAUD
#define ISP_UART_ERROR (ISP_UART_ACTUAL_BAUD - ISP_UART_BAUD)
#else
#define ISP_UART_ERROR (ISP_UART_BAUD - ISP_UART_ACTUAL_BAUD)
#endif
#if ISP_UART_ERROR * 100UL > ISP_UART_BAUD * 2UL
#error UART divisor error exceeds two percent. Review clock/baud together.
#endif
#define ISP_UART_SDR ((ISP_UART_DIVISOR - 1UL) << 9)
#endif
