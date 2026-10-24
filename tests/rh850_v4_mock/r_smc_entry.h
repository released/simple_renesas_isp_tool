#ifndef RH850_V4_TEST_ENTRY_H
#define RH850_V4_TEST_ENTRY_H
#include <stdint.h>
#include "iodefine.h"
#undef PORT
#undef INTC1
#undef CLKCTL
typedef struct {
    uint16_t PM0,PMC0,PFC0,PFCE0,PFCAE0,PM10,PMC10,PFC10,PFCE10,PFCAE10;
} test_ports_t;
typedef struct {
    uint16_t MKRCANGRECC0,RFRCANGRECC0,TBRCANGRECC0,CTRCANGRECC0;
    uint16_t P3RCANGRECC0,P2RCANGRECC0,P1RCANGRECC0,P0RCANGRECC0;
} test_irq_bits_t;
typedef struct {test_irq_bits_t BIT;} test_irq_t;
typedef struct {test_irq_t ICRCANGRECC0;} test_intc_t;
typedef struct {
    uint32_t PLL1C,PLL1S,CKSC_CPUCLKS_ACT,CKSC_CPUCLKD_ACT;
    uint32_t CKSC_PPLLCLKS_ACT,CKSC_IPERI2S_ACT,CKSC_ICANS_ACT;
} test_clock_t;
extern test_ports_t g_test_ports;
extern test_intc_t g_test_intc;
extern test_clock_t g_test_clock;
extern uint32_t g_test_psw;
#define PORT g_test_ports
#define INTC1 g_test_intc
#define CLKCTL g_test_clock
#define DI() ((void)(g_test_psw|=0x20UL))
#define EI() ((void)(g_test_psw&=~0x20UL))
#define __stsr_rh(a,b) (g_test_psw)
#define __syncp() ((void)0)
#define _CGC_RSCAN_CLK_SOURCE_PPLLCLK (3UL)
#endif
