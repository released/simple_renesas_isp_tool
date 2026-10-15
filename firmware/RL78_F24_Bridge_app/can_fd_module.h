/*_____ I N C L U D E S ____________________________________________________*/

#ifndef CAN_FD_MODULE_H
#define CAN_FD_MODULE_H

#include "r_rscanfd_rl78_if.h"
#include "r_rscanfd_rl78_config.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/

typedef struct
{
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t rx_lost_frames;
    uint32_t tx_failures;
    uint32_t warning_entries;
    uint32_t error_passive_entries;
    uint32_t bus_off_entries;
    uint32_t recovery_count;
    uint32_t arbitration_lost_entries;
    uint16_t last_global_error;
    uint16_t last_channel_error;
    uint16_t last_transmit_source;
    uint16_t last_api_error;
} can_host_stats_t;

typedef struct
{
    uint16_t channel_sources;
    uint16_t global_sources;
    uint32_t controller_state;
    uint32_t arbitration_lost_entries;
} can_host_error_event_t;

typedef enum
{
    CAN_HOST_RESULT_OK = 0,
    CAN_HOST_RESULT_BAD_ARGUMENT,
    CAN_HOST_RESULT_BAD_STATE,
    CAN_HOST_RESULT_DRIVER_ERROR
} can_host_result_t;

typedef struct
{
    uint8_t mode;
    uint8_t nominal_bitrate_preset;
    uint8_t data_bitrate_preset;
    uint8_t flags;
} can_host_config_t;

typedef struct
{
    uint8_t enabled;
    uint8_t ide;
    uint8_t frame_type;
    uint32_t id;
    uint32_t mask;
} can_host_filter_t;

typedef struct
{
    uint16_t input_head[4];
    uint16_t input_data[4];
    uint16_t ram_idl;
    uint16_t ram_idh;
    uint8_t ram_dlc_register;
    uint8_t ram_fd_control;
    uint16_t ram_data[4];
    uint16_t ram_window_before;
} can_host_tx_diagnostic_t;

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void CAN_Get_Module_Version(void);

void CAN_Get_Timestamp_Counter(void);

void CAN_Get_Transceiver_Delay_Compensation(void);

void CAN_Get_Bus_Error_Flag(void);

void CAN_Get_Channel_Status(void);

void CAN_Read_RX_Data_Common_FIFO(void);

void CAN_Read_RX_Data_FIFO(void);

void CAN_Get_RX_Data_Message_Buffer(void);

void CAN_Read_TX_History_list(void);

void CAN_Abort_TX_Data_Common_FIFO(void);

void CAN_Send_TX_Data_Common_FIFO(void);

void CAN_Get_TX_Data_Message_Buffer_Result(void);

void CAN_Abort_TX_Data_Message_Buffer(void);


void CAN_FD_Send_TX_Data_Message_Buffer2(void);
void CAN_FD_Send_TX_Data_Message_Buffer1(void);
void CAN_Send_TX_Data_Message_Buffer_polling2(void);
void CAN_Send_TX_Data_Message_Buffer_polling1(void);
void CAN_Send_TX_Data_Message_Buffer(void);

void CAN_Bus_EntrySleepMode(void);

void CAN_Bus_Reset(void);

e_can_err_t CAN_Bus_Config(void);
e_can_err_t CAN_Transition_Check(void);

can_host_result_t CAN_HostSetConfig(const can_host_config_t *config);
can_host_result_t CAN_HostSetFilter(uint8_t index,
                                    const can_host_filter_t *filter);
can_host_result_t CAN_HostStart(uint32_t *controller_state);
can_host_result_t CAN_HostStop(uint32_t *controller_state);
uint8_t CAN_HostIsRunning(void);
uint32_t CAN_HostGetControllerState(void);
void CAN_HostGetConfig(can_host_config_t *config);
can_host_result_t CAN_HostTransmit(const st_can_tx_frame_t *frame);
void CAN_HostGetLastTxDiagnostic(can_host_tx_diagnostic_t *diagnostic);
uint8_t CAN_HostPollTransmit(e_can_txb_result_t *result,
                             uint16_t *timestamp);
can_host_result_t CAN_HostAbortTransmit(void);
void CAN_HostDiscardRxQueue(void);

uint8_t CAN_RxQueuePop(st_can_rx_frame_t *frame, can_length_t *length);
void CAN_GetHostStats(can_host_stats_t *stats);
void CAN_ClearHostStats(void);
uint8_t CAN_HostTakeErrorEvent(can_host_error_event_t *event);

extern void my_can_glb_rxfifo_callback(uint16_t arg);
extern void my_can_glb_rxmb_callback(uint16_t arg);
extern void my_can_glb_error_callback(uint16_t arg);
extern void my_can_ch0_transmit_callback(uint16_t arg);
extern void my_can_ch0_cfifo_rx_callback(uint16_t arg);
extern void my_can_ch0_error_callback(uint16_t arg);

#endif
