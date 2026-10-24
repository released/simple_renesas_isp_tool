#ifndef RH850_MODEL_ENTRY_H
#define RH850_MODEL_ENTRY_H
#include <stdint.h>
#define __syncp() ((void)0)
typedef struct { uint16_t MKRCANGRECC0,RFRCANGRECC0,TBRCANGRECC0,CTRCANGRECC0;
    uint16_t P3RCANGRECC0,P2RCANGRECC0,P1RCANGRECC0,P0RCANGRECC0; } mock_bits_t;
typedef struct { mock_bits_t BIT; } mock_int_t;
typedef struct { mock_int_t ICRCANGRECC0; } mock_intc_t;
typedef struct { uint32_t UINT32; } mock_reg_t;
typedef struct { mock_reg_t ncfg,ctr,sts,erfl; } mock_channel_t;
typedef struct { mock_reg_t fifo[8]; mock_channel_t channels[6];
    uint8_t tx_status[192]; } mock_can_t;
extern volatile mock_intc_t INTC1;
extern volatile mock_can_t RCFDC0;
#endif
