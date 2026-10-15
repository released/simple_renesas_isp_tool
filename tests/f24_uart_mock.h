#ifndef F24_UART_MOCK_H
#define F24_UART_MOCK_H
#include <stdint.h>
typedef uint16_t MD_STATUS;
#define MD_OK (0U)
#define BSP_DI() ((void)0)
#define BSP_EI() ((void)0)
extern volatile uint16_t SDR00, SDR01;
uint16_t get_tick(void);
void R_Config_UART0_Stop(void);
void R_Config_UART0_Start(void);
MD_STATUS R_Config_UART0_Send(uint8_t *data, uint16_t count);
#endif
