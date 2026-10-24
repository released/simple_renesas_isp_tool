#ifndef RH850_PORTS_MODEL_CAN_H
#define RH850_PORTS_MODEL_CAN_H
#include "r_smc_entry.h"
#include "../rh850_mock/rs_canfd.h"
#undef RS_CANFD_TX_BUFFER_TMSTS
#define RS_CANFD_TX_BUFFER_TMSTS(unit) ((unit)->tx_status)
#define RS_CANFD_RX_FIFO_STS(unit) ((unit)->fifo)
#define RS_CANFD_CHANNEL_REGS(unit) ((unit)->channels)
#endif
