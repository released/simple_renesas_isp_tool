#ifndef F13_BOARD_MOCK_H
#define F13_BOARD_MOCK_H
#define __near
#define F13_BOARD_CLOCK_POLL_LIMIT (20UL)
#define F13_BOARD_CAN_POLL_LIMIT (20UL)

typedef union
{
    unsigned char value;
    struct
    {
        unsigned char b0:1;
        unsigned char b1:1;
        unsigned char b2:1;
        unsigned char b3:1;
        unsigned char b4:1;
        unsigned char b5:1;
        unsigned char b6:1;
        unsigned char b7:1;
    } bits;
} f13_mock_reg8_t;

extern volatile f13_mock_reg8_t f13_pllctl, f13_ckc;
#define PLLCTL f13_pllctl.value
#define PLLON f13_pllctl.bits.b0
#define SELPLL f13_pllctl.bits.b2
#define CKC f13_ckc.value
#define MCM0 f13_ckc.bits.b4
#define CSS f13_ckc.bits.b7
extern volatile unsigned char CMC, MSTOP, HIOSTOP, SELLOSC, MDIV, XTSTOP;
extern volatile unsigned char RTCCL, TRD_CKSEL, OSTS, OSTC, PLLSTS;
extern volatile unsigned char P1, PM1, PSRSEL, CAN0EN;
extern volatile unsigned char CAN0TRMMK, CAN0CFRMK, CAN0ERRMK, CANGRFRMK, CANGERRMK;
extern volatile unsigned short GSTS, C0CTRL, C0STSL, GCTRL;
void F13MockNop(void);
#define NOP() F13MockNop()
#endif
