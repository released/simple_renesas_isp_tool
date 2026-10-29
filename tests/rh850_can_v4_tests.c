/* Actual v4 service/driver/config/ISP wrapper, with modeled wire/board edges.
 * A renamed production board adapter is also linked for register-side tests.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "r_smc_entry.h"
#include "can_service.h"
#include "can_board.h"
#include "rh850_can_ports.h"
#ifdef TEST_DEVICE_ADAPTER
#include "device_platform.h"
void Device_CanRxIsr(void);
#endif
#ifdef TEST_CLASSICAL_ADAPTER
#include "rh850_can.h"
void rs_canfd_rx_fifo_interrupt(void);
#endif
rs_canfd_result_t Actual_Prepare(void);
rs_canfd_result_t Actual_Start(const rs_canfd_config_t *config);
rs_canfd_result_t Actual_Stop(void);
rs_canfd_result_t Actual_StopChannel(uint8_t channel);
rs_canfd_result_t Actual_StartChannel(uint8_t channel);
rs_canfd_result_t Actual_Send(uint8_t channel,const rs_canfd_frame_t *frame);
rs_canfd_result_t Actual_TxResult(uint8_t channel,rs_canfd_tx_result_t *result);
uint32_t Actual_Lock(void);
void Actual_Unlock(uint32_t saved);
void Actual_ClearErrors(uint8_t channel,uint32_t global,uint32_t errors);
rs_canfd_unit_t g_can_test_registers;
test_ports_t g_test_ports;
test_intc_t g_test_intc;
test_clock_t g_test_clock;
uint32_t g_test_psw;
static uint32_t g_tick,g_step,g_status[6],g_flags[6];
static rs_canfd_frame_t g_rx[8][40],g_sent[6];
static unsigned g_heads[8],g_tails[8],g_reads[8],g_sends[6],g_aborts[6],g_stops[6],g_starts[6];
static rs_canfd_tx_result_t g_tx[6];
static uint8_t g_pending,g_lost[8],g_tx_fault,g_abort_confirms,g_fd;
static unsigned g_checks, g_board_stops;
static rs_canfd_result_t g_prepare_result;
#define CHECK(c) do {g_checks++;if(!(c)){printf("FAIL line%d: %s\n",__LINE__,#c);exit(1);}}while(0)

unsigned long Device_Millis(void)
{
    uint8_t i,mode;
    g_tick+=g_step;
    for(i=0U;i<6U;i++) {
        mode=(uint8_t)(RS_CANFD_CHANNEL_REGS(&RCFDC0)[i].ctr.UINT32&3UL);
        RS_CANFD_CHANNEL_REGS(&RCFDC0)[i].sts.UINT32=mode==1U?1UL:(mode==2U?2UL:0UL);
    }
    mode=(uint8_t)(RCFDC0.CFDGCTR.UINT32&3UL);
    RCFDC0.CFDGSTS.UINT32=mode==1U?1UL:(mode==2U?2UL:0UL);
    return (unsigned long)g_tick;
}
uint32_t CanBoard_Millis(void){return (uint32_t)Device_Millis();}
uint32_t CanBoard_Lock(void){uint32_t saved;saved=g_test_psw;DI();return saved;}
void CanBoard_Unlock(uint32_t saved){if(!(saved&0x20UL)){EI();}}
rs_canfd_result_t CanBoard_Prepare(void){return g_prepare_result;}
rs_canfd_result_t CanBoard_Start(const rs_canfd_config_t *config)
{
    uint8_t i;
    CHECK(config->channel_count==CAN_ENABLED_COUNT);
    CHECK(config->rx_buffer.count==0U);
    for(i=0U;i<CAN_ENABLED_COUNT;i++) {
        CHECK(config->channels[i].channel==g_can_profiles[i].channel);
        CHECK(config->channels[i].rx_fifo_count==1U);
        CHECK(config->channels[i].rx_fifos[0].fifo==g_can_profiles[i].channel);
        CHECK(config->channels[i].rx_fifos[0].interrupt_enable==1U);
        CHECK(config->channels[i].receive_filters[0].id_mask==0x7FFUL);
        CHECK(config->channels[i].receive_filters[0].frame_match==RS_CANFD_RECEIVE_FRAME_DATA);
    }
    return RS_CANFD_OK;
}
rs_canfd_result_t CanBoard_Stop(void){g_board_stops++;return RS_CANFD_OK;}
rs_canfd_result_t CanBoard_StopChannel(uint8_t ch){g_stops[ch]++;return RS_CANFD_OK;}
rs_canfd_result_t CanBoard_StartChannel(uint8_t ch){g_starts[ch]++;g_status[ch]=0UL;return RS_CANFD_OK;}
void CanBoard_EnableRxIrq(uint8_t enable){g_test_intc.ICRCANGRECC0.BIT.MKRCANGRECC0=(uint16_t)!enable;}
void CanBoard_AckRxIrq(uint8_t pending){g_pending=pending;}
uint8_t CanBoard_TakeFifoLost(uint8_t fifo){uint8_t n;n=g_lost[fifo];g_lost[fifo]=0U;return n;}
rs_canfd_result_t CanBoard_ReadFifo(uint8_t fifo,rs_canfd_frame_t *out)
{
    g_reads[fifo]++;
    if(g_heads[fifo]==g_tails[fifo]){return RS_CANFD_ERR_EMPTY;}
    *out=g_rx[fifo][g_tails[fifo]++%40U];return RS_CANFD_OK;
}
rs_canfd_result_t CanBoard_ReadLatest(uint8_t buffer,rs_canfd_frame_t *out)
{(void)buffer;(void)out;return RS_CANFD_ERR_STATE;}
rs_canfd_result_t CanBoard_Send(uint8_t ch,const rs_canfd_frame_t *frame)
{g_sends[ch]++;g_sent[ch]=*frame;g_tx[ch]=g_tx_fault?RS_CANFD_TX_PENDING:RS_CANFD_TX_COMPLETE;return RS_CANFD_OK;}
rs_canfd_result_t CanBoard_TxResult(uint8_t ch,rs_canfd_tx_result_t *out){*out=g_tx[ch];return RS_CANFD_OK;}
rs_canfd_result_t CanBoard_Abort(uint8_t ch)
{g_aborts[ch]++;if(g_abort_confirms){g_tx[ch]=RS_CANFD_TX_ABORTED;}return RS_CANFD_OK;}
rs_canfd_result_t CanBoard_Status(uint8_t ch,rs_canfd_status_t *out)
{memset(out,0,sizeof(*out));out->channel_status=g_status[ch];out->channel_error_flags=g_flags[ch];return RS_CANFD_OK;}
void CanBoard_ClearErrors(uint8_t ch,uint32_t global,uint32_t errors)
{(void)ch;(void)global;(void)errors;}
static void Push(uint8_t ch,uint8_t n)
{
    rs_canfd_frame_t *f;
    f=&g_rx[ch][g_heads[ch]++%40U];rs_canfd_frame_clear(f);
    f->id=0x700UL;f->length=8U;f->dlc=8U;f->data[0]=ch;f->data[1]=n;
    f->frame_mode=g_fd?RS_CANFD_FRAME_FD:RS_CANFD_FRAME_CLASSIC;f->brs=g_fd;
}
static void Init(void)
{
    CanPorts_Stop();memset(g_heads,0,sizeof(g_heads));memset(g_tails,0,sizeof(g_tails));
    g_tx_fault=0U;g_abort_confirms=1U;g_prepare_result=RS_CANFD_OK;
    CHECK(CanPorts_Init(g_fd,0x700U,1U,0x720U,g_fd?0x7FFU:0x7F8U));
}
int main(int argc,char **argv)
{
    uint8_t i,ch,first,last,data[64];
    unsigned total,before,got[6];
    can_port_frame_t frame;
    can_port_diagnostics_t diag;
    can_diagnostics_t detail;
    rs_canfd_frame_t queued;
    rs_canfd_config_report_t report;
    can_tx_event_t event;
    uint16_t tag;
    uint32_t saved;
    rs_canfd_tx_result_t actual_tx;
#ifdef TEST_DEVICE_ADAPTER
    boot_frame_t wrapped;
#endif
#ifdef TEST_CLASSICAL_ADAPTER
    device_can_frame_t wrapped;
#endif
    g_fd=(uint8_t)(argc>1&&argv[1][0]=='1');g_step=1UL;
    /* A valid-image reset path has never opened CAN in this Boot instance.
     * Stopping it must not access clock-gated hardware or run register waits.
     */
    before=g_board_stops;
    CanPorts_Stop();
    CHECK(g_board_stops==before);
    for(ch=0U;ch<6U;ch++){CHECK(CanPorts_Enabled(ch)==((CAN_ENABLED_MASK&(1U<<ch))!=0U));}
    memset(data,0,sizeof(data));memset(got,0,sizeof(got));Init();
    before=g_board_stops;
    CanPorts_Stop();
    CHECK(g_board_stops==before+1U && !Can_IsReady());
    CanPorts_Stop();
    CHECK(g_board_stops==before+1U);
    Init();
    first=CanPorts_First();last=first;
    CHECK(CanConfig_Build(&report)==RS_CANFD_OK);
    CHECK(report.message_ram_bytes==CAN_ENABLED_COUNT*16UL*(12UL+(g_fd?64UL:8UL)));
    CHECK(report.rule_count==CAN_ENABLED_COUNT*2U);
    for(i=0U;i<CAN_ENABLED_COUNT;i++){ch=g_can_profiles[i].channel;last=ch;Push(ch,0U);Push(ch,1U);}
    CanPorts_Irq();total=0U;
    while(CanPorts_Read(&frame)) {
        CHECK(frame.channel==frame.data[0]&&frame.data[1]==got[frame.channel]++);
        CHECK(CanPorts_Send(frame.channel,0x701U,frame.data,frame.length));
        CHECK(g_sent[frame.channel].id==0x701UL&&g_sent[frame.channel].brs==g_fd);total++;
    }
    CHECK(total==CAN_ENABLED_COUNT*2U);
    before=g_reads[first];CHECK(!CanPorts_Read(&frame)&&g_reads[first]==before);
    for(i=0U;i<32U;i++){Push(first,i);}if(last!=first){Push(last,0U);}
    CanPorts_Irq();CHECK(g_pending);CanPorts_Irq();CanPorts_Diagnostics(first,&diag);CHECK(diag.dropped>=1U);
    if(last!=first){CanPorts_Diagnostics(last,&diag);CHECK(diag.dropped==0U);}
    while(CanPorts_Read(&frame)){}CanPorts_Irq();while(CanPorts_Read(&frame)){}
    Push(first,0U);g_rx[first][(g_heads[first]-1U)%40U].brs=(uint8_t)!g_fd;
    CanPorts_Irq();CHECK(!CanPorts_Read(&frame));
    if(g_fd){Push(first,0U);g_rx[first][(g_heads[first]-1U)%40U].length=64U;
        g_rx[first][(g_heads[first]-1U)%40U].dlc=15U;g_rx[first][(g_heads[first]-1U)%40U].data[63]=0xA5U;
        CanPorts_Irq();CHECK(CanPorts_Read(&frame)&&frame.data[63]==0xA5U);}
    g_test_psw=0x20UL;CanPorts_Diagnostics(first,&diag);CHECK(g_test_psw==0x20UL);
    g_flags[first]=0x123UL;CHECK(CanPorts_ErrorFlags(first)==0x123UL);CHECK(g_flags[first]==0x123UL);
    g_test_psw=0UL;
    g_tx_fault=1U;before=g_aborts[first];CHECK(!CanPorts_Send(first,0x701U,data,8U));
    CHECK(g_aborts[first]==before+1U);CanPorts_Diagnostics(first,&diag);CHECK(diag.last_result==RS_CANFD_ERR_TIMEOUT);
    g_abort_confirms=0U;CHECK(!CanPorts_Send(first,0x701U,data,8U));
    CHECK(Can_GetDiagnostics(first,&detail)==RS_CANFD_OK&&detail.tx_fault);
    before=g_sends[first];CHECK(!CanPorts_Send(first,0x701U,data,8U)&&g_sends[first]==before);
    if(last!=first){g_tx_fault=0U;CHECK(CanPorts_Send(last,0x701U,data,8U));CHECK(Can_ChannelRunning(last));}
    CHECK(Can_RecoverChannel(first)==RS_CANFD_OK);CHECK(Can_ChannelRunning(first));
    g_tx_fault=0U;CHECK(CanPorts_Send(first,0x701U,data,8U));
    rs_canfd_frame_clear(&queued);queued.id=0x701UL;queued.length=8U;queued.frame_mode=g_fd?RS_CANFD_FRAME_FD:RS_CANFD_FRAME_CLASSIC;queued.brs=g_fd;
    CHECK(Can_Send(first,&queued,10U)==RS_CANFD_OK);CHECK(Can_Send(first,&queued,11U)==RS_CANFD_OK);
    CHECK(Can_CancelTx(first,10U)==RS_CANFD_OK);
    CHECK(Can_TakeTxEvent(first,&tag,&event)==RS_CANFD_OK&&tag==10U&&event==CAN_TX_CANCELLED);
    Can_Process();Can_Process();CHECK(Can_TakeTxEvent(first,&tag,&event)==RS_CANFD_OK&&tag==11U&&event==CAN_TX_DONE);
    CHECK(Can_StopChannel(first)==RS_CANFD_OK);CHECK(!Can_ChannelRunning(first));
    if(last!=first){CHECK(Can_ChannelRunning(last));CHECK(CanPorts_Send(last,0x701U,data,8U));}
    CHECK(Can_RecoverChannel(first)==RS_CANFD_OK);
    /* A stale/unrelated event must not authorize this protocol reply. */
    CHECK(Can_Send(first,&queued,99U)==RS_CANFD_OK);Can_Process();Can_Process();
    CHECK(!CanPorts_Send(first,0x701U,data,8U));CHECK(!Can_ChannelRunning(first));
    CHECK(Can_RecoverChannel(first)==RS_CANFD_OK);
    /* Production EVM board pin/clock/lock and retained-error behavior. */
    memset(&g_test_ports,0xFF,sizeof(g_test_ports));g_test_clock.PLL1C=0x1031DUL;
    g_test_clock.PLL1S=4UL;g_test_clock.CKSC_CPUCLKS_ACT=3UL;g_test_clock.CKSC_CPUCLKD_ACT=0x11UL;
    g_test_clock.CKSC_PPLLCLKS_ACT=3UL;g_test_clock.CKSC_IPERI2S_ACT=1UL;g_test_clock.CKSC_ICANS_ACT=3UL;
    CHECK(Actual_Prepare()==RS_CANFD_OK);
    if(CanPorts_Enabled(1U)){CHECK((PORT.PM10&0xC0U)==0x40U);}
    if(CanPorts_Enabled(4U)){CHECK((PORT.PM0&0x600U)==0x200U);}
    saved=Actual_Lock();CHECK(g_test_psw&0x20UL);Actual_Unlock(saved);CHECK(!(g_test_psw&0x20UL));
    g_test_psw=0x20UL;saved=Actual_Lock();Actual_Unlock(saved);CHECK(g_test_psw==0x20UL);g_test_psw=0UL;
    RCFDC0.CFDGERFL.UINT32=0x55UL;RS_CANFD_CHANNEL_REGS(&RCFDC0)[first].erfl.UINT32=0x66UL;
    Actual_ClearErrors(first,0x55UL,0x66UL);CHECK(RCFDC0.CFDGERFL.UINT32==0x55UL&&RS_CANFD_CHANNEL_REGS(&RCFDC0)[first].erfl.UINT32==0x66UL);
    g_test_clock.CKSC_ICANS_ACT=0UL;CHECK(Actual_Prepare()==RS_CANFD_ERR_STATE);CHECK(INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0);
    g_test_clock.CKSC_ICANS_ACT=3UL;
    CHECK(Actual_Start(CanConfig_Get())==RS_CANFD_OK);
    for(i=0U;i<CAN_ENABLED_COUNT;i++) {
        ch=g_can_profiles[i].channel;
        RS_CANFD_TX_BUFFER_TMSTS(&RCFDC0)[ch*32U]=0U;
        CHECK(Actual_Send(ch,&queued)==RS_CANFD_OK);
        RS_CANFD_TX_BUFFER_TMSTS(&RCFDC0)[ch*32U]=4U;
        CHECK(Actual_TxResult(ch,&actual_tx)==RS_CANFD_OK&&actual_tx==RS_CANFD_TX_COMPLETE);
    }
    CHECK(Actual_StopChannel(first)==RS_CANFD_OK);CHECK(Actual_StartChannel(first)==RS_CANFD_OK);CHECK(Actual_Stop()==RS_CANFD_OK);
    CanPorts_Stop();CHECK(!CanPorts_Init(g_fd,0x800U,0U,0U,0U));Init();
#ifdef TEST_DEVICE_ADAPTER
    CanPorts_Stop();CHECK(Device_CanInit(0x700U,1U));
    for(i=0U;i<CAN_ENABLED_COUNT;i++){Push(g_can_profiles[i].channel,0U);}Device_CanRxIsr();total=0U;
    while(Device_CanRead(&wrapped)){CHECK(wrapped.channel==wrapped.data[0]);CHECK(Device_CanSend(0x701U,wrapped.data,wrapped.length));total++;}
    CHECK(total==CAN_ENABLED_COUNT);Device_CanStop();
#endif
#ifdef TEST_CLASSICAL_ADAPTER
    CanPorts_Stop();CHECK(Device_CanInit(0x700U));
    for(i=0U;i<CAN_ENABLED_COUNT;i++){Push(g_can_profiles[i].channel,0U);}rs_canfd_rx_fifo_interrupt();total=0U;
    while(Device_CanRead(&wrapped)){CHECK(wrapped.channel==wrapped.data[0]);CHECK(Device_CanSend(0x701U,wrapped.data));total++;}
    CHECK(total==CAN_ENABLED_COUNT);Device_CanStop();
#endif
    printf("PASS RH850 ISP/v4: %u assertions, %u ports, %s; physical bus NOT RUN.\n",g_checks,(unsigned)CAN_ENABLED_COUNT,g_fd?"FD":"Classical");
    return 0;
}
