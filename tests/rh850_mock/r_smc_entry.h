#ifndef RH850_MODEL_ENTRY_H
#define RH850_MODEL_ENTRY_H
#include <stdint.h>
#define __syncp() ((void)0)
typedef struct { uint16_t MKRCANGRECC0,RFRCANGRECC0,TBRCANGRECC0,CTRCANGRECC0;
    uint16_t P3RCANGRECC0,P2RCANGRECC0,P1RCANGRECC0,P0RCANGRECC0;
    uint16_t MKRCANGERR0,MKRCAN0ERR,MKRCAN2ERR; } mock_bits_t;
typedef struct { mock_bits_t BIT; } mock_int_t;
typedef struct { mock_int_t ICRCANGRECC0,ICRCANGERR0,ICRCAN0ERR,ICRCAN2ERR; } mock_intc_t;
typedef struct { uint32_t UINT32; } mock_reg_t;
typedef struct { mock_reg_t CFDRFSTS0; uint8_t tx_status; } mock_can_t;
extern volatile mock_intc_t INTC1,INTC2;
extern volatile mock_can_t RCFDC0;
#endif
