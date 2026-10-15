#ifndef F24_CLOCK_TEST_PLATFORM_H
#define F24_CLOCK_TEST_PLATFORM_H
#include <stdint.h>
#define __near
#include "../../firmware/f24_board/f24_board_config.h"

typedef union
{
    uint8_t byte;
    struct
    {
        uint8_t b0:1, b1:1, b2:1, b3:1, b4:1, b5:1, b6:1, b7:1;
    } bits;
} mock_register_t;

extern volatile mock_register_t mock_ckc;
extern volatile mock_register_t mock_cksel;
extern volatile mock_register_t mock_pllctl;
extern volatile mock_register_t mock_pllsts;
extern volatile uint8_t MDIV, HIOSTOP, MSTOP, OSTS, OSTC;
extern volatile uint16_t GSTS, C0STSL, C0NCFGH, C0NCFGL;
/* Read-only CMC expression makes a runtime rewrite a compile error. */
uint8_t MockClockReadCmc(void);
#define CMC MockClockReadCmc()
#define CKC mock_ckc.byte
#define CKSEL mock_cksel.byte
#define PLLCTL mock_pllctl.byte
#define PLLSTS mock_pllsts.byte
#define MCM0 mock_ckc.bits.b4
#define MCS mock_ckc.bits.b5
#define CLS mock_ckc.bits.b7
#define FMAINDIV0 mock_cksel.bits.b5
#define FMAINDIV1 mock_cksel.bits.b6
#define FPLLDIV mock_cksel.bits.b7
#define PLLON mock_pllctl.bits.b0
#define SELPLL mock_pllctl.bits.b2
#define SELPLLS mock_pllsts.bits.b3
void MockClockAdvance(void);
#define BSP_NOP() MockClockAdvance()
#endif
