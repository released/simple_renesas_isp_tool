#ifndef F24_CAN_CLOCK_H
#define F24_CAN_CLOCK_H

#include <stdint.h>

#define F24_CAN_CLOCK_READY                 (0U)
#define F24_CAN_CLOCK_NOT_STARTED           (1U)
#define F24_CAN_CLOCK_X1_TIMEOUT            (2U)
#define F24_CAN_CLOCK_MAIN_TIMEOUT          (3U)
#define F24_CAN_CLOCK_PLL_TIMEOUT           (4U)
#define F24_CAN_CLOCK_SELECT_TIMEOUT        (5U)
#define F24_CAN_CLOCK_BAD_STARTUP           (6U)
#define F24_CAN_CLOCK_FALLBACK_TIMEOUT      (7U)

/* This board revision uses a user-confirmed 20 MHz X1 crystal. RS-CANFD lite
 * requires fCLK = fMP/2 and forbids HOCO as the communication clock.
 * Keep generated startup at HOCO 40 MHz for reset-to-main debugging.
 * CS+ preincludes f24_board_config.h: BSP selects X1 mode at its one CMC
 * write, but defers oscillator startup until main. Before UART/timer/CAN,
 * switch X1 20 MHz /4 x16 -> PLL 80 MHz -> CPU 40 MHz. Missing X1/PLL must
 * leave Host Link available on HOCO 40 MHz for normal timeout failures.
 * This routine must only be called before peripheral start, with IRQs off. */
uint8_t F24_CanClockInit(void);
uint8_t F24_CanClockIsReady(void);
void F24_CanClockGetDiagnostic(uint8_t *payload);

#endif
