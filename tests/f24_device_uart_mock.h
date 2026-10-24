#ifndef F24_DEVICE_UART_MOCK_H
#define F24_DEVICE_UART_MOCK_H
#include <stdint.h>
#define NOP() ((void)0)
extern uint8_t SAU1EN, STMK1, SRMK1, STIF1, SRIF1;
extern uint8_t PIOR4, PMC7, P7, PM7, NFEN0;
extern uint8_t STPR11, STPR01, SRPR11, SRPR01, SDR10L, SDR11L;
extern uint16_t ST1, SPS1, SMR10, SCR10, SDR10, SMR11, SCR11, SDR11;
extern uint16_t SIR11, SSR11, SO1, SOL1, SOE1, SS1;
uint8_t F24_CanClockIsReady(void);
#endif
