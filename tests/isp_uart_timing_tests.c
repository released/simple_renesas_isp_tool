#include "isp_uart_timing.h"
typedef char FitsSdr[((ISP_UART_SDR & ~0xFE00UL) == 0UL) ? 1 : -1];
int main(void){return (ISP_UART_ERROR * 100UL <= ISP_UART_BAUD * 2UL) ? 0 : 1;}
