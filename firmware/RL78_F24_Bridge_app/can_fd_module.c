 /*_____ I N C L U D E S ____________________________________________________*/
#include <string.h>

#include "inc_main.h"
#include "can_fd_module.h"
#include "f24_can_clock.h"
#include "f24_can_timing.h"

#if CAN_CFG_DCS != 0
#error F24_bridge_timing_requires_PLL_peripheral_clock_not_X1_bypass
#endif

/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/
typedef unsigned short WCHAR;


st_can_rx_frame_t rx_frame;

#define CAN_RX_QUEUE_CAPACITY                 (4U)
#define CAN_RX_QUEUE_STORAGE_DEPTH            (CAN_RX_QUEUE_CAPACITY + 1U)
#define CAN_TRANSITION_POLL_LIMIT             (60000UL)
#define CAN_F13_BOOT_RESPONSE_ID              (0x701UL)
#define CAN_F13_BOOT_RESPONSE_MASK            (0x7FFUL)

#define CAN_HOST_MODE_CLASSICAL               (0U)
#define CAN_HOST_MODE_FD                      (1U)
#define CAN_HOST_NOMINAL_PRESET_500K          (2U)
#define CAN_HOST_DATA_PRESET_2M               (1U)
#define CAN_HOST_DATA_PRESET_UNUSED           (0xFFU)
#define CAN_TRANSCEIVER_NORMAL_LEVEL          (0U)
#define CAN_TRANSCEIVER_STANDBY_LEVEL         (1U)
#define CAN_RAM_WINDOW_KEY                    (0xC400U)
#define CAN_RAM_WINDOW_PAGE1                  (0xC401U)

static st_can_rx_frame_t g_can_rx_queue[CAN_RX_QUEUE_STORAGE_DEPTH];
static can_length_t g_can_rx_lengths[CAN_RX_QUEUE_STORAGE_DEPTH];
static st_can_rx_frame_t g_can_discard_frame;
static volatile uint8_t g_can_rx_head = 0U;
static volatile uint8_t g_can_rx_tail = 0U;
static volatile can_host_stats_t g_can_stats;
static can_host_config_t g_can_config = {
    CAN_HOST_MODE_CLASSICAL,
    CAN_HOST_NOMINAL_PRESET_500K,
    CAN_HOST_DATA_PRESET_UNUSED,
    0U
};
static can_host_filter_t g_can_filter = {
    1U,
    CAN_IDE_STD_FORMAT,
    CAN_RTR_DATA_FRAME,
    CAN_F13_BOOT_RESPONSE_ID,
    CAN_F13_BOOT_RESPONSE_MASK
};
static uint8_t g_can_initialized = 0U;
static uint8_t g_can_config_valid = 0U;
static uint8_t g_can_running = 0U;
static volatile uint8_t g_can_tx_pending = 0U;
static volatile uint8_t g_can_tx_event = 0U;
static volatile e_can_txb_result_t g_can_tx_event_result = CAN_TXB_TRANSMITTING;
static volatile uint16_t g_can_tx_timestamp = 0U;
static volatile uint16_t g_can_pending_global_errors = 0U;
static volatile uint16_t g_can_pending_channel_errors = 0U;
static can_host_tx_diagnostic_t g_can_last_tx_diagnostic;

/*_____ M A C R O S ________________________________________________________*/
#define __T(x)      L ## x
#define _T(x)       __T(x)
/*_____ F U N C T I O N S __________________________________________________*/

void can_fd_err_code(e_can_err_t rc)
{
    g_can_stats.last_api_error = (uint16_t)rc;
}


void can_fd_txb_err_code(e_can_txb_result_t rc)
{
    g_can_stats.last_api_error = (uint16_t)rc;
    if ((rc == CAN_TXB_ARG_ERROR) ||
        (rc == CAN_TXB_ABORT_OVER) ||
        (rc == CAN_TXB_END_WITH_ABORT))
    {
        if (g_can_stats.tx_failures != 0xFFFFFFFFUL)
        {
            g_can_stats.tx_failures++;
        }
    }
}


void CAN_Get_Module_Version(void)
{
    unsigned short result; 
    result = R_CAN_GetVersion(); 

    (void)(result);
}

void CAN_Get_Timestamp_Counter(void)
{
    unsigned char value; 
    value = R_CAN_GetTSCounter(); 

    (void)(value);
}

void CAN_Get_Transceiver_Delay_Compensation(void)
{
    unsigned char result; 
    result = R_CAN_GetTDCResult(); 

    (void)(result);
}

void CAN_Get_Bus_Error_Flag(void)
{
    unsigned char errflag; 
    errflag = R_CAN_GetChBusErrFlag();  

    (void)(errflag);
}

void CAN_Get_Channel_Status(void)
{
    unsigned short sts; 
    sts = R_CAN_GetChStatus(); 

    (void)(sts);
}

void CAN_Read_RX_Data_Common_FIFO(void)
{
    e_can_err_t       rtn; 
    // st_can_rx_frame_t rx_frame; 
    can_length_t      length; 

    rtn = R_CAN_ReadCFIFO(&rx_frame, &length);  
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }
    else 
    { 
        /* process for receive message */ 
    } 
}

void CAN_Read_RX_Data_FIFO(void)
{
    e_can_err_t       rtn; 
    // st_can_rx_frame_t rx_frame; 
    can_length_t      length; 

    rtn = R_CAN_ReadRXMB(CAN_RXFIFO0, &rx_frame, &length); 
    if (rtn == CAN_SUCCESS) 
    { 
        /* process for receive message */ 
    } 
    else
    {
        // can_fd_err_code(rtn); 
        return;
    }

}

void decode_rcv_frame(st_can_rx_frame_t * pFrame)
{
    (void)pFrame;
}


void CAN_Get_RX_Data_Message_Buffer(void)
{
    e_can_err_t       rtn; 
    // st_can_rx_frame_t rx_frame; 
    can_length_t      length; 
    // unsigned char i = 0;

    rtn = R_CAN_ReadRXMB(CAN_RXBUF0, &rx_frame, &length);  
    if (rtn != CAN_SUCCESS)
    {
        // can_fd_err_code(rtn); 
    } 
    else
    {
        decode_rcv_frame(&rx_frame);
    }
}

void CAN_Read_TX_History_list(void)
{
    e_can_err_t       rtn; 
    st_can_txhist_t entry; 

    rtn = R_CAN_ReadTxHistory(&entry);  
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    } 
    else
    { 
        if (entry.txbuf_idx == CAN_TXBUF_NOT) 
        { 
            /* process for TX history of CFIFO */ 
        } 
        else 
        { 
            /* process for TX history of TXMB */ 
        } 
    } 
}

void CAN_Abort_TX_Data_Common_FIFO(void)
{
    e_can_err_t  rtn; 
    rtn = R_CAN_AbortCFIFO();   
 
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    } 

    (void)(rtn);
}

void CAN_Send_TX_Data_Common_FIFO(void)
{
    e_can_err_t rtn; 
    st_can_tx_frame_t s_tx_frame;
    unsigned char i = 0;

    s_tx_frame.Head.Bits.IDE   = CAN_IDE_STD_FORMAT; 
    s_tx_frame.Head.Bits.ID    = 0x7FEuL; 
    s_tx_frame.Head.Bits.RTR   = CAN_RTR_DATA_FRAME; 
    s_tx_frame.Head.Bits.FDCTR = CAN_FDCTR_CLASSICAL; 
    s_tx_frame.Head.Bits.DLC   = CAN_DLC_LEN8; 
    s_tx_frame.Head.Bits.THLEN   = CAN_THL_DISABLE; 
    s_tx_frame.Head.Bits.LBL  = 0u; 
    s_tx_frame.Head.Bits.IFL   = 0u; 
    for (i = 0u; i < CAN_DLC_LEN8; i++)  s_tx_frame.Data.DB[i] = 0x22u * (i + 1); 
    rtn = R_CAN_SendByCFIFO(&s_tx_frame); 
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    } 

    (void)(rtn);
}

void CAN_Get_TX_Data_Message_Buffer_Result(void)
{
    e_can_txb_result_t txb_rtn; 
    txb_rtn = R_CAN_GetTXMBResult(CAN_TXBUF0);  
    if (txb_rtn != CAN_TXB_END)
    {
        can_fd_txb_err_code(txb_rtn);
    } 

    (void)(txb_rtn);
}

void CAN_Abort_TX_Data_Message_Buffer(void)
{
    e_can_err_t rtn; 
    rtn = R_CAN_AbortTXMB(CAN_TXBUF0);    
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }   

    (void)(rtn);
}



void CAN_FD_Send_TX_Data_Message_Buffer2(void)
{
    static uint8_t cnt = 0;
    uint8_t i = 0;
    e_can_err_t rtn; 
    e_can_txb_result_t txb_rtn; 
    st_can_tx_frame_t s_tx_frame;
    unsigned char DLC = CAN_FD_DLC_LEN32;
    unsigned char len = 32;
    unsigned char txbuf_idx = CAN_TXBUF0;

    s_tx_frame.Head.Bits.IDE   = CAN_IDE_STD_FORMAT; 
    s_tx_frame.Head.Bits.ID    = 0x140uL; //0x7FEuL; 
    s_tx_frame.Head.Bits.RTR   = CAN_RTR_DATA_FRAME;    //CAN_RTR_ANY_FRAME; 
    s_tx_frame.Head.Bits.FDCTR = CAN_FDCTR_FD_ESI; 
    s_tx_frame.Head.Bits.DLC   = DLC; 
    s_tx_frame.Head.Bits.THLEN   = CAN_THL_DISABLE; 
    s_tx_frame.Head.Bits.LBL  = 0u; 
    s_tx_frame.Head.Bits.IFL   = 0u; 
 
    len = 32;
    for (i = 0;i < 32 ; i++)
    {
        s_tx_frame.Data.DB[i] = i;
    }
    s_tx_frame.Data.DB[0] = 0x12;
    s_tx_frame.Data.DB[1] = 0x34;
    s_tx_frame.Data.DB[2] = cnt;

    s_tx_frame.Data.DB[len-2] = 0x56;
    s_tx_frame.Data.DB[len-1] = 0x78;

    rtn = R_CAN_SendByTXMB(txbuf_idx, &s_tx_frame);   

    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }   
    else
    {
        txb_rtn = R_CAN_GetTXMBResult(txbuf_idx);   
        if (txb_rtn != CAN_TXB_END)
        {            
            can_fd_txb_err_code(txb_rtn);
        }
    }

    (void)(rtn);

    cnt++;
}


void CAN_FD_Send_TX_Data_Message_Buffer1(void)
{
    static uint8_t cnt = 0;
    uint8_t i = 0;
    e_can_err_t rtn; 
    e_can_txb_result_t txb_rtn; 
    st_can_tx_frame_t s_tx_frame;
    unsigned char DLC = CAN_FD_DLC_LEN64;
    unsigned char len = 64;
    unsigned char txbuf_idx = CAN_TXBUF0;

    s_tx_frame.Head.Bits.IDE   = CAN_IDE_EXT_FORMAT; 
    s_tx_frame.Head.Bits.ID    = 0x130uL; //0x7FEuL; 
    s_tx_frame.Head.Bits.RTR   = CAN_RTR_DATA_FRAME;    //CAN_RTR_ANY_FRAME; 
    s_tx_frame.Head.Bits.FDCTR = CAN_FDCTR_FD; 
    s_tx_frame.Head.Bits.DLC   = DLC; 
    s_tx_frame.Head.Bits.THLEN   = CAN_THL_DISABLE; 
    s_tx_frame.Head.Bits.LBL  = 0u; 
    s_tx_frame.Head.Bits.IFL   = 0u; 
 
    //len = CAN_FD_DLC_LEN64;
    for (i = 0;i < 64 ; i++)
    {
        s_tx_frame.Data.DB[i] = i;
    }
    s_tx_frame.Data.DB[0] = 0xAA;
    s_tx_frame.Data.DB[1] = 0x55;
    s_tx_frame.Data.DB[2] = cnt;

    s_tx_frame.Data.DB[len-2] = 0x55;
    s_tx_frame.Data.DB[len-1] = 0xAA;

    rtn = R_CAN_SendByTXMB(txbuf_idx, &s_tx_frame);   

    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }   
    else
    {
        txb_rtn = R_CAN_GetTXMBResult(txbuf_idx);   
        if (txb_rtn != CAN_TXB_END)
        {            
            can_fd_txb_err_code(txb_rtn);
        }
    }

    (void)(rtn);

    cnt++;
}



void CAN_Send_TX_Data_Message_Buffer_polling2(void)
{
    e_can_err_t rtn; 
    e_can_txb_result_t txb_rtn; 
    st_can_tx_frame_t s_tx_frame;
    unsigned char len = CAN_DLC_LEN8;
    unsigned char txbuf_idx = CAN_TXBUF0;

    static uint8_t cnt = 0;

    s_tx_frame.Head.Bits.IDE   = CAN_IDE_EXT_FORMAT; 
    s_tx_frame.Head.Bits.ID    = 0x110uL; //0x7FEuL; 
    s_tx_frame.Head.Bits.RTR   = CAN_RTR_DATA_FRAME;    //CAN_RTR_ANY_FRAME; 
    s_tx_frame.Head.Bits.FDCTR = CAN_FDCTR_CLASSICAL; 
    s_tx_frame.Head.Bits.DLC   = len; 
    s_tx_frame.Head.Bits.THLEN   = CAN_THL_DISABLE; 
    s_tx_frame.Head.Bits.LBL  = 0u; 
    s_tx_frame.Head.Bits.IFL   = 0u; 
 
    s_tx_frame.Data.DB[0] = 0x5A;
    s_tx_frame.Data.DB[1] = 0x5A;
    s_tx_frame.Data.DB[2] = 0x00 + cnt;
    s_tx_frame.Data.DB[3] = 0x01 + cnt;    
    s_tx_frame.Data.DB[4] = 0x02 + cnt;
    s_tx_frame.Data.DB[5] = 0x03 + cnt;
    s_tx_frame.Data.DB[6] = 0xA5;
    s_tx_frame.Data.DB[7] = 0xA5;

    rtn = R_CAN_SendByTXMB(txbuf_idx, &s_tx_frame);   

    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }   
    else
    {
        txb_rtn = R_CAN_GetTXMBResult(txbuf_idx);   
        if (txb_rtn != CAN_TXB_END)
        {            
            can_fd_txb_err_code(txb_rtn);
        }
    }

    (void)(rtn);
    
    cnt = (cnt >= 0xFF) ? (0x00) : (cnt + 0x10);
}

void CAN_Send_TX_Data_Message_Buffer_polling1(void)
{
    e_can_err_t rtn; 
    e_can_txb_result_t txb_rtn; 
    st_can_tx_frame_t s_tx_frame;
    unsigned char len = CAN_DLC_LEN8;
    unsigned char txbuf_idx = CAN_TXBUF0;

    static uint8_t cnt = 0;

    s_tx_frame.Head.Bits.IDE   = CAN_IDE_STD_FORMAT; 
    s_tx_frame.Head.Bits.ID    = 0x55uL; //0x7FEuL; 
    s_tx_frame.Head.Bits.RTR   = CAN_RTR_DATA_FRAME;    //CAN_RTR_ANY_FRAME; 
    s_tx_frame.Head.Bits.FDCTR = CAN_FDCTR_CLASSICAL; 
    s_tx_frame.Head.Bits.DLC   = len; 
    s_tx_frame.Head.Bits.THLEN   = CAN_THL_DISABLE; 
    s_tx_frame.Head.Bits.LBL  = 0u; 
    s_tx_frame.Head.Bits.IFL   = 0u; 
 
    s_tx_frame.Data.DB[0] = 0x5A;
    s_tx_frame.Data.DB[1] = 0x5A;
    s_tx_frame.Data.DB[2] = 0x00 + cnt;
    s_tx_frame.Data.DB[3] = 0x01 + cnt;    
    s_tx_frame.Data.DB[4] = 0x02 + cnt;
    s_tx_frame.Data.DB[5] = 0x03 + cnt;
    s_tx_frame.Data.DB[6] = 0xA5;
    s_tx_frame.Data.DB[7] = 0xA5;

    rtn = R_CAN_SendByTXMB(txbuf_idx, &s_tx_frame);   

    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }   
    else
    {
        txb_rtn = R_CAN_GetTXMBResult(txbuf_idx);   
        if (txb_rtn != CAN_TXB_END)
        {            
            can_fd_txb_err_code(txb_rtn);
        }
    }

    (void)(rtn);
    
    cnt++;
}

void CAN_Send_TX_Data_Message_Buffer(void)
{
    e_can_err_t rtn; 
    e_can_txb_result_t txb_rtn; 
    st_can_tx_frame_t s_tx_frame;
    unsigned char len = CAN_DLC_LEN8;
    unsigned char txbuf_idx = CAN_TXBUF0;

    s_tx_frame.Head.Bits.IDE   = CAN_IDE_EXT_FORMAT; 
    s_tx_frame.Head.Bits.ID    = 0x120uL; //0x7FEuL; 
    s_tx_frame.Head.Bits.RTR   = CAN_RTR_DATA_FRAME;    //CAN_RTR_ANY_FRAME; 
    s_tx_frame.Head.Bits.FDCTR = CAN_FDCTR_CLASSICAL; 
    s_tx_frame.Head.Bits.DLC   = len; 
    s_tx_frame.Head.Bits.THLEN   = CAN_THL_DISABLE; 
    s_tx_frame.Head.Bits.LBL  = 0u; 
    s_tx_frame.Head.Bits.IFL   = 0u; 
 
    s_tx_frame.Data.DB[0] = 0x12;
    s_tx_frame.Data.DB[1] = 0x34;
    s_tx_frame.Data.DB[2] = 0x56;
    s_tx_frame.Data.DB[3] = 0x78;    
    s_tx_frame.Data.DB[4] = 0x9A;
    s_tx_frame.Data.DB[5] = 0xBC;
    s_tx_frame.Data.DB[6] = 0xDE;
    s_tx_frame.Data.DB[7] = 0xF0;

    rtn = R_CAN_SendByTXMB(txbuf_idx, &s_tx_frame);   

    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    }   
    else
    {
        txb_rtn = R_CAN_GetTXMBResult(txbuf_idx);   
        if (txb_rtn != CAN_TXB_END)
        {            
            can_fd_txb_err_code(txb_rtn);
        }
    }

    (void)(rtn);
}


void CAN_Bus_EntrySleepMode(void)
{
    e_can_err_t rtn; 
    rtn = R_CAN_Sleep(CAN_WUP_UNUSE);  
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    } 

    (void)(rtn);
}

void CAN_Bus_Reset(void)
{
    e_can_err_t rtn; 
    rtn = R_CAN_StopComm();  
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    } 

    (void)(rtn);
}

static void can_increment_counter(volatile uint32_t *counter)
{
    if (*counter != 0xFFFFFFFFUL)
    {
        (*counter)++;
    }
}

e_can_err_t CAN_Transition_Check(void)
{
    e_can_err_t rtn;
    uint32_t poll_count;

    rtn = CAN_ERR_WAITING;
    poll_count = 0UL;
    while ((CAN_ERR_WAITING == rtn) &&
           (poll_count < CAN_TRANSITION_POLL_LIMIT))
    {
        rtn = R_CAN_StartComm();
        poll_count++;
    }

    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn);
        return rtn;
    }

    poll_count = 0UL;
    while ((0x0080U != R_CAN_GetChStatus()) &&
           (poll_count < CAN_TRANSITION_POLL_LIMIT))
    {
        poll_count++;
    }
    if (poll_count >= CAN_TRANSITION_POLL_LIMIT)
    {
        can_fd_err_code(CAN_ERR_WAITING);
        return CAN_ERR_WAITING;
    }

    return CAN_SUCCESS;
}

uint8_t CAN_RxQueuePop(st_can_rx_frame_t *frame, can_length_t *length)
{
    uint8_t tail;

    if ((frame == (st_can_rx_frame_t *)0) ||
        (length == (can_length_t *)0))
    {
        return 0U;
    }

    tail = g_can_rx_tail;
    if (tail == g_can_rx_head)
    {
        return 0U;
    }

    *frame = g_can_rx_queue[tail];
    *length = g_can_rx_lengths[tail];
    tail++;
    if (tail >= CAN_RX_QUEUE_STORAGE_DEPTH)
    {
        tail = 0U;
    }
    g_can_rx_tail = tail;
    return 1U;
}

void CAN_GetHostStats(can_host_stats_t *stats)
{
    if (stats == (can_host_stats_t *)0)
    {
        return;
    }

    BSP_DI();
    stats->rx_frames = g_can_stats.rx_frames;
    stats->tx_frames = g_can_stats.tx_frames;
    stats->rx_lost_frames = g_can_stats.rx_lost_frames;
    stats->tx_failures = g_can_stats.tx_failures;
    stats->warning_entries = g_can_stats.warning_entries;
    stats->error_passive_entries = g_can_stats.error_passive_entries;
    stats->bus_off_entries = g_can_stats.bus_off_entries;
    stats->recovery_count = g_can_stats.recovery_count;
    stats->arbitration_lost_entries =
        g_can_stats.arbitration_lost_entries;
    stats->last_global_error = g_can_stats.last_global_error;
    stats->last_channel_error = g_can_stats.last_channel_error;
    stats->last_transmit_source = g_can_stats.last_transmit_source;
    stats->last_api_error = g_can_stats.last_api_error;
    BSP_EI();
}

void CAN_ClearHostStats(void)
{
    BSP_DI();
    memset((void *)&g_can_stats, 0, sizeof(g_can_stats));
    g_can_pending_global_errors = 0U;
    g_can_pending_channel_errors = 0U;
    BSP_EI();
}

uint8_t CAN_HostTakeErrorEvent(can_host_error_event_t *event)
{
    uint16_t global_sources;
    uint16_t channel_sources;
    uint32_t arbitration_lost_entries;

    if (event == (can_host_error_event_t *)0)
    {
        return 0U;
    }

    BSP_DI();
    global_sources = g_can_pending_global_errors;
    channel_sources = g_can_pending_channel_errors;
    arbitration_lost_entries = g_can_stats.arbitration_lost_entries;
    g_can_pending_global_errors = 0U;
    g_can_pending_channel_errors = 0U;
    BSP_EI();

    if ((global_sources == 0U) && (channel_sources == 0U))
    {
        return 0U;
    }

    event->global_sources = global_sources;
    event->channel_sources = channel_sources;
    event->controller_state = CAN_HostGetControllerState();
    event->arbitration_lost_entries = arbitration_lost_entries;
    return 1U;
}

static void CAN_RxQueuePushFromIsr(uint16_t source)
{
    uint8_t head;
    uint8_t next;
    st_can_rx_frame_t *destination;
    can_length_t length;
    e_can_err_t result;

    (void)source;
    head = g_can_rx_head;
    next = (uint8_t)(head + 1U);
    if (next >= CAN_RX_QUEUE_STORAGE_DEPTH)
    {
        next = 0U;
    }

    if (next == g_can_rx_tail)
    {
        destination = &g_can_discard_frame;
    }
    else
    {
        destination = &g_can_rx_queue[head];
    }

    result = R_CAN_ReadRXMBInHandler(CAN_RXBUF0, destination, &length);
    if ((result == CAN_SUCCESS) || (result == CAN_SUCCESS_WITH_LOST))
    {
        if ((next == g_can_rx_tail) || (result == CAN_SUCCESS_WITH_LOST))
        {
            can_increment_counter(&g_can_stats.rx_lost_frames);
        }
        if (next != g_can_rx_tail)
        {
            g_can_rx_lengths[head] = length;
            g_can_rx_head = next;
            can_increment_counter(&g_can_stats.rx_frames);
        }
    }
    else
    {
        g_can_stats.last_api_error = (uint16_t)result;
    }
}

static void CAN_ClearRxQueue(void)
{
    /* The caller keeps the CAN controller stopped and RX interrupts masked. */
    g_can_rx_head = 0U;
    g_can_rx_tail = 0U;
}

void CAN_HostDiscardRxQueue(void)
{
    BSP_DI();
    g_can_rx_head = g_can_rx_tail;
    BSP_EI();
}

static e_can_err_t CAN_SetRX_Rule(void)
{
    e_can_err_t rtn; 
    st_can_filter_t filter; 
    st_can_filter_opt_t opt; 
    unsigned char rxbuf_idx = 0;

    if (g_can_filter.enabled == 0U)
    {
        return CAN_SUCCESS;
    }

    filter.IDE_TYPE = g_can_filter.ide;
    filter.RTR_TYPE = g_can_filter.frame_type;
    filter.ID = g_can_filter.id;
    filter.ID_MASK = g_can_filter.mask;
    opt.DLC = CAN_DLC_LEN8;
    opt.LB = CAN_AFL_LB_NOT_LOOPBACK;
    opt.LBL = 0U;
    opt.IFL = 0U;
    rtn = R_CAN_AddRxRule(&filter, CAN_STORE_RM, rxbuf_idx, &opt);
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn); 
    } 

    return rtn;
}

static uint8_t CAN_IsSupportedConfig(const can_host_config_t *config)
{
    f24_can_nominal_timing_t timing;
    if ((config->flags != 0U) ||
        (F24_CanTimingGet(config->nominal_bitrate_preset, &timing) == 0U))
    {
        return 0U;
    }
    if (config->mode == CAN_HOST_MODE_CLASSICAL)
    {
        if (config->data_bitrate_preset != CAN_HOST_DATA_PRESET_UNUSED)
        {
            return 0U;
        }
    }
    else if (config->mode == CAN_HOST_MODE_FD)
    {
        if (config->data_bitrate_preset != CAN_HOST_DATA_PRESET_2M)
        {
            return 0U;
        }
    }
    else
    {
        return 0U;
    }

    return 1U;
}

can_host_result_t CAN_HostSetConfig(const can_host_config_t *config)
{
    if (config == (const can_host_config_t *)0)
    {
        return CAN_HOST_RESULT_BAD_ARGUMENT;
    }
    if ((g_can_initialized == 0U) || (g_can_running != 0U))
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }
    if (CAN_IsSupportedConfig(config) == 0U)
    {
        return CAN_HOST_RESULT_BAD_ARGUMENT;
    }

    if (F24_CanTimingApply(config->nominal_bitrate_preset) == 0U)
    {
        return CAN_HOST_RESULT_DRIVER_ERROR;
    }
    g_can_config = *config;
    g_can_config_valid = 1U;
    return CAN_HOST_RESULT_OK;
}

can_host_result_t CAN_HostSetFilter(uint8_t index,
                                    const can_host_filter_t *filter)
{
    uint32_t maximum_id;

    if ((filter == (const can_host_filter_t *)0) || (index != 0U))
    {
        return CAN_HOST_RESULT_BAD_ARGUMENT;
    }
    if ((g_can_initialized == 0U) || (g_can_running != 0U))
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }
    if ((filter->enabled > 1U) || (filter->ide > 1U) ||
        (filter->frame_type > 1U))
    {
        return CAN_HOST_RESULT_BAD_ARGUMENT;
    }

    maximum_id = (filter->ide == CAN_IDE_STD_FORMAT) ?
                 0x7FFUL : 0x1FFFFFFFUL;
    if ((filter->id > maximum_id) || (filter->mask > maximum_id) ||
        ((filter->enabled != 0U) && (filter->mask == 0UL)))
    {
        return CAN_HOST_RESULT_BAD_ARGUMENT;
    }

    g_can_filter = *filter;
    return CAN_HOST_RESULT_OK;
}

uint32_t CAN_HostGetControllerState(void)
{
    return (uint32_t)R_CAN_GetChStatus();
}

uint8_t CAN_HostIsRunning(void)
{
    return g_can_running;
}

void CAN_HostGetConfig(can_host_config_t *config)
{
    if (config != (can_host_config_t *)0)
    {
        *config = g_can_config;
    }
}

can_host_result_t CAN_HostTransmit(const st_can_tx_frame_t *frame)
{
    e_can_err_t result;
    uint16_t ram_window;

    if (frame == (const st_can_tx_frame_t *)0)
    {
        return CAN_HOST_RESULT_BAD_ARGUMENT;
    }
    if ((g_can_initialized == 0U) || (g_can_running == 0U))
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }
    if (F24_CanClockIsReady() == 0U)
    {
        return CAN_HOST_RESULT_DRIVER_ERROR;
    }
    if (g_can_tx_pending != 0U)
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }

    g_can_tx_event = 0U;
    g_can_tx_event_result = CAN_TXB_TRANSMITTING;
    g_can_tx_pending = 1U;
    memset(&g_can_last_tx_diagnostic, 0,
           sizeof(g_can_last_tx_diagnostic));
    memcpy(g_can_last_tx_diagnostic.input_head, frame->Head.Word,
           sizeof(g_can_last_tx_diagnostic.input_head));
    memcpy(g_can_last_tx_diagnostic.input_data, frame->Data.DW,
           sizeof(g_can_last_tx_diagnostic.input_data));
    result = R_CAN_SendByTXMB(CAN_TXBUF0, frame);
    if (result != CAN_SUCCESS)
    {
        g_can_tx_pending = 0U;
        can_fd_err_code(result);
        can_increment_counter(&g_can_stats.tx_failures);
        return CAN_HOST_RESULT_DRIVER_ERROR;
    }

    /* Read the TX message RAM back through the same page used by the driver.
     * This diagnostic is captured outside the ISR and does not affect UART. */
    ram_window = CFDGRWC;
    g_can_last_tx_diagnostic.ram_window_before = ram_window;
    CFDGRWC = CAN_RAM_WINDOW_PAGE1;
    g_can_last_tx_diagnostic.ram_idl = TMID0L;
    g_can_last_tx_diagnostic.ram_idh = TMID0H;
    g_can_last_tx_diagnostic.ram_dlc_register = TMPTR0HH;
    g_can_last_tx_diagnostic.ram_fd_control = TMFDCTR0LL;
    g_can_last_tx_diagnostic.ram_data[0] = TMDF0_0L;
    g_can_last_tx_diagnostic.ram_data[1] = TMDF0_0H;
    g_can_last_tx_diagnostic.ram_data[2] = TMDF0_1L;
    g_can_last_tx_diagnostic.ram_data[3] = TMDF0_1H;
    CFDGRWC = (uint16_t)(CAN_RAM_WINDOW_KEY | ram_window);
    return CAN_HOST_RESULT_OK;
}

void CAN_HostGetLastTxDiagnostic(can_host_tx_diagnostic_t *diagnostic)
{
    if (diagnostic != (can_host_tx_diagnostic_t *)0)
    {
        memcpy(diagnostic, &g_can_last_tx_diagnostic,
               sizeof(*diagnostic));
    }
}

uint8_t CAN_HostPollTransmit(e_can_txb_result_t *result,
                             uint16_t *timestamp)
{
    e_can_txb_result_t polled_result;

    if ((result == (e_can_txb_result_t *)0) ||
        (timestamp == (uint16_t *)0))
    {
        return 0U;
    }

    if (g_can_tx_event != 0U)
    {
        BSP_DI();
        *result = g_can_tx_event_result;
        *timestamp = g_can_tx_timestamp;
        g_can_tx_event = 0U;
        BSP_EI();
        return 1U;
    }
    if (g_can_tx_pending == 0U)
    {
        return 0U;
    }

    polled_result = R_CAN_GetTXMBResult(CAN_TXBUF0);
    if (polled_result == CAN_TXB_TRANSMITTING)
    {
        return 0U;
    }

    g_can_tx_pending = 0U;
    *result = polled_result;
    *timestamp = R_CAN_GetTSCounter();
    if (polled_result == CAN_TXB_END)
    {
        can_increment_counter(&g_can_stats.tx_frames);
    }
    else
    {
        can_fd_txb_err_code(polled_result);
    }
    return 1U;
}

can_host_result_t CAN_HostAbortTransmit(void)
{
    e_can_err_t result;

    if (g_can_tx_pending == 0U)
    {
        return CAN_HOST_RESULT_OK;
    }
    result = R_CAN_AbortTXMB(CAN_TXBUF0);
    if (result != CAN_SUCCESS)
    {
        can_fd_err_code(result);
        can_increment_counter(&g_can_stats.tx_failures);
        return CAN_HOST_RESULT_DRIVER_ERROR;
    }
    return CAN_HOST_RESULT_OK;
}

can_host_result_t CAN_HostStart(uint32_t *controller_state)
{
    e_can_err_t result;

    if ((g_can_initialized == 0U) || (g_can_config_valid == 0U) ||
        (g_can_filter.enabled == 0U) ||
        (F24_CanClockIsReady() == 0U) ||
        (CAN_IsSupportedConfig(&g_can_config) == 0U))
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }
    if (g_can_running != 0U)
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }

    P1_bit.no2 = CAN_TRANSCEIVER_NORMAL_LEVEL;
    result = R_CAN_StopComm();
    if (result == CAN_SUCCESS)
    {
        result = R_CAN_SetConfig();
    }
    if ((result == CAN_SUCCESS) &&
        (F24_CanTimingApply(g_can_config.nominal_bitrate_preset) == 0U))
    {
        result = CAN_ERR_INVALID_MODE;
    }
    if (result == CAN_SUCCESS)
    {
        result = CAN_SetRX_Rule();
    }
    if (result == CAN_SUCCESS)
    {
        CAN_ClearRxQueue();
        result = CAN_Transition_Check();
    }
    if (result != CAN_SUCCESS)
    {
        P1_bit.no2 = CAN_TRANSCEIVER_STANDBY_LEVEL;
        can_fd_err_code(result);
        return CAN_HOST_RESULT_DRIVER_ERROR;
    }

    g_can_running = 1U;
    g_can_tx_pending = 0U;
    g_can_tx_event = 0U;
    if (controller_state != (uint32_t *)0)
    {
        *controller_state = CAN_HostGetControllerState();
    }
    return CAN_HOST_RESULT_OK;
}

can_host_result_t CAN_HostStop(uint32_t *controller_state)
{
    e_can_err_t result;

    if (g_can_initialized == 0U)
    {
        return CAN_HOST_RESULT_BAD_STATE;
    }
    if (g_can_running != 0U)
    {
        result = R_CAN_StopComm();
        if (result != CAN_SUCCESS)
        {
            can_fd_err_code(result);
            return CAN_HOST_RESULT_DRIVER_ERROR;
        }
        g_can_running = 0U;
        g_can_tx_pending = 0U;
        g_can_tx_event = 0U;
        CAN_ClearRxQueue();
    }
    P1_bit.no2 = CAN_TRANSCEIVER_STANDBY_LEVEL;

    if (controller_state != (uint32_t *)0)
    {
        *controller_state = CAN_HostGetControllerState();
    }
    return CAN_HOST_RESULT_OK;
}

/*
    check AN : RS-CANFD Lite Configuration, Reception and Transmission
    Table 1.4 Communication Speed Calculation and Examples of Communication Speed (Nominal) 
    Table 1.5 Communication Speed Calculation and Examples of Communication Speed 
    (Nominal and Data) 
    Table 1.6 Example of Bit Timing Settings 
    
    @40MHz ,
    when 500k bps : 8Tq(10) , 20Tq(4) 
    when 250k bps : 8Tq(20) , 20Tq(8) 

    - 8Tq(10) , sample rate 75%  : SS:1 , TSEG1:5 , TSEG2:2 , SJW:1
    - 20Tq(4) , sample rate 70%  : SS:1 , TSEG1:13 , TSEG2:6 , SJW:1 
    - 20Tq(4) , sample rate 80%  : SS:1 , TSEG1:15 , TSEG2:4 , SJW:3     

    set 250K @ sample rate 75 
*/
void CAN_Speed_Config(void)
{

}

e_can_err_t CAN_Bus_Config(void)
{
    e_can_err_t rtn;

    /* Keep the active-high STB pin asserted until CAN_START succeeds. */
    P1_bit.no2 = CAN_TRANSCEIVER_STANDBY_LEVEL;
    (void)R_CAN_GetVersion();

    g_can_initialized = 0U;
    g_can_config_valid = 0U;
    g_can_running = 0U;
    g_can_tx_pending = 0U;
    g_can_tx_event = 0U;
    if (F24_CanClockInit() != F24_CAN_CLOCK_READY)
    {
        return CAN_ERR_INVALID_MODE;
    }
    R_CAN_Create();

    rtn = R_CAN_StopComm();  
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn);
        return rtn;
    }

    rtn = R_CAN_SetConfig(); 
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn);
        return rtn;
    }

    rtn = CAN_SetRX_Rule();
    if (rtn != CAN_SUCCESS)
    {
        can_fd_err_code(rtn);
        return rtn;
    }

    CAN_ClearRxQueue();
    P1_bit.no2 = CAN_TRANSCEIVER_STANDBY_LEVEL;
    g_can_initialized = 1U;
    return CAN_SUCCESS;
}


void my_can_glb_rxfifo_callback(uint16_t arg)
{
    (void)arg;
}

void my_can_glb_rxmb_callback(uint16_t arg)
{
    CAN_RxQueuePushFromIsr(arg);
}

void my_can_glb_error_callback(uint16_t arg)
{
    g_can_stats.last_global_error = arg;
    g_can_pending_global_errors |= arg;
}

void my_can_ch0_transmit_callback(uint16_t arg)
{
    g_can_stats.last_transmit_source = arg;
    if ((arg & CAN_SOURCE_TRMC_TM0) != 0U)
    {
        g_can_tx_event_result = CAN_TXB_END;
        can_increment_counter(&g_can_stats.tx_frames);
    }
    else
    {
        g_can_tx_event_result = CAN_TXB_ABORT_OVER;
        can_increment_counter(&g_can_stats.tx_failures);
    }
    g_can_tx_timestamp = R_CAN_GetTSCounter();
    g_can_tx_pending = 0U;
    g_can_tx_event = 1U;
}

void my_can_ch0_cfifo_rx_callback(uint16_t arg)
{
    (void)arg;
}

void my_can_ch0_error_callback(uint16_t arg)
{
    g_can_stats.last_channel_error = arg;
    g_can_pending_channel_errors |= arg;
    if ((arg & CAN_SOURCE_CH_ERR_WARNING) != 0U)
    {
        can_increment_counter(&g_can_stats.warning_entries);
    }
    if ((arg & CAN_SOURCE_CH_ERR_PASSIVE) != 0U)
    {
        can_increment_counter(&g_can_stats.error_passive_entries);
    }
    if ((arg & CAN_SOURCE_CH_ERR_BO_ENTRY) != 0U)
    {
        can_increment_counter(&g_can_stats.bus_off_entries);
    }
    if ((arg & CAN_SOURCE_CH_ERR_BO_RCVR) != 0U)
    {
        can_increment_counter(&g_can_stats.recovery_count);
    }
    if ((arg & CAN_SOURCE_CH_ERR_ARB_LOST) != 0U)
    {
        can_increment_counter(&g_can_stats.arbitration_lost_entries);
    }
}

