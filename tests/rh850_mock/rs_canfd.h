#ifndef RH850_MODEL_CAN_H
#define RH850_MODEL_CAN_H
#include "r_smc_entry.h"
typedef enum { RS_CANFD_OK,RS_CANFD_ERR_PARAM,RS_CANFD_ERR_STATE,RS_CANFD_ERR_TIMEOUT,
    RS_CANFD_ERR_BUSY,RS_CANFD_ERR_EMPTY,RS_CANFD_ERR_FULL } rs_canfd_result_t;
#define RS_CANFD_ID_STANDARD 0
#define RS_CANFD_FRAME_DATA 0
#define RS_CANFD_FRAME_CLASSIC 0
#define RS_CANFD_FRAME_FD 1
#define RS_CANFD_CHANNEL_FD_ONLY 1
#define RS_CANFD_CHANNEL_CLASSIC_ONLY 2
#define RS_CANFD_PAYLOAD_64_BYTES 7
#define RS_CANFD_PAYLOAD_8_BYTES 0
#define RS_CANFD_FIFO_DEPTH_16 2
#define RS_CANFD_FIFO_INT_FULL 0
#define RS_CANFD_CFDRFSTS_RFMLT 4UL
#define RS_CANFD_CFDRFSTS_RFIF 8UL
#define RS_CANFD_CFDTMSTS_TMTSTS 1U
#define RS_CANFD_TX_BUFFER_TMSTS(unit) (&((unit)->tx_status))
#define RS_CANFD_BIT_TIMING_40MHZ_N500K_D2M {0,58,19,19,0,13,4,4}
typedef struct { uint32_t id; uint8_t data[64]; uint8_t id_format,frame_type,frame_mode,brs,dlc,length; } rs_canfd_frame_t;
typedef struct { uint16_t id,mask; uint8_t data_only,minimum; } rs_canfd_receive_filter_t;
typedef struct { uint8_t fifo,enable,interrupt_enable,interrupt_at_every_frame,interrupt_timing,depth,payload; } rs_canfd_rx_fifo_config_t;
typedef struct { uint16_t nominal_brp; uint8_t nominal_tseg1,nominal_tseg2,nominal_sjw;
    uint16_t data_brp; uint8_t data_tseg1,data_tseg2,data_sjw; } rs_canfd_bit_timing_t;
typedef struct { uint8_t channel,enable,fd_mode; rs_canfd_bit_timing_t bit_timing;
    const rs_canfd_receive_filter_t *receive_filters; uint8_t receive_filter_count;
    struct {uint8_t count,payload;} rx_buffer; const rs_canfd_rx_fifo_config_t *rx_fifos;
    uint8_t rx_fifo_count; } rs_canfd_channel_config_t;
typedef struct {volatile mock_can_t *unit; struct { uint8_t tx_priority_by_id; } global;
    const rs_canfd_channel_config_t *channels; uint8_t channel_count;} rs_canfd_config_t;
rs_canfd_result_t rs_canfd_receive_filter_accept_standard_id_to_rx_fifo(rs_canfd_receive_filter_t*,uint16_t,uint16_t,uint8_t);
void rs_canfd_receive_filter_accept_data_frames(rs_canfd_receive_filter_t*);
void rs_canfd_receive_filter_set_minimum_dlc(rs_canfd_receive_filter_t*,uint8_t);
rs_canfd_result_t rs_canfd_init(const rs_canfd_config_t*);
rs_canfd_result_t rs_canfd_read_rx_fifo(volatile mock_can_t*,uint8_t,rs_canfd_frame_t*);
rs_canfd_result_t rs_canfd_send_tx_buffer(volatile mock_can_t*,uint8_t,uint8_t,const rs_canfd_frame_t*);
rs_canfd_result_t rs_canfd_abort_tx_buffer(volatile mock_can_t*,uint8_t,uint8_t);
rs_canfd_result_t rs_canfd_deinit(volatile mock_can_t*);
uint8_t rs_canfd_dlc_to_length(uint8_t);
uint8_t rs_canfd_length_to_dlc(uint8_t);
#endif
