/* Production multi-port adapter, simulated register and driver boundaries. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "r_smc_entry.h"
#include "rs_canfd.h"
#include "rh850_can_ports.h"
#ifdef TEST_DEVICE_ADAPTER
#include "device_platform.h"
void Device_CanRxIsr(void);
#endif
#ifdef TEST_CLASSICAL_ADAPTER
#include "rh850_can.h"
void rs_canfd_rx_fifo_interrupt(void);
#endif

volatile mock_intc_t INTC1;
volatile mock_can_t RCFDC0;
static rs_canfd_frame_t input[6][40];
static unsigned char counts[6], positions[6], seen[6];
static unsigned reads, failures, initialized, sent_port, aborted_port, pin_calls;
static unsigned long tick;
static unsigned char expected_fd, tx_fault;
static unsigned char expected_raw=1U;

static void Check(int ok,const char *name)
{
    if(!ok){failures++;printf("FAIL: %s\n",name);}
}
unsigned long Device_Millis(void){return tick++;}
void R_PORT_SetAltFunc(int port,int pin,int alternate,int dir)
{
    static const int pins[6][6]={{0,0,2,0,1,7},{10,7,4,10,6,4},
        {0,4,1,0,5,7},{0,8,5,0,7,7},{0,10,5,0,9,5},{0,14,5,0,13,7}};
    unsigned char ch;
    int match;
    match=0;
    for(ch=0;ch<6;ch++){
        if(!CanPorts_Enabled(ch)){continue;}
        if(dir==1 && port==pins[ch][0] && pin==pins[ch][1] && alternate==pins[ch][2]){match=1;}
        if(dir==0 && port==pins[ch][3] && pin==pins[ch][4] && alternate==pins[ch][5]){match=1;}
    }
    Check(match,"only enabled channel pins");pin_calls++;
}
uint8_t rs_canfd_dlc_to_length(uint8_t d)
{
    static const uint8_t lengths[16]={0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    return d<16?lengths[d]:0;
}
uint8_t rs_canfd_length_to_dlc(uint8_t n){return n<=8?n:15;}
rs_canfd_result_t rs_canfd_receive_filter_accept_standard_id_to_rx_fifo(
    rs_canfd_receive_filter_t *rule,uint16_t id,uint16_t mask,uint8_t fifo)
{
    Check(CanPorts_Enabled(fifo),"filter uses its channel FIFO");
    memset(rule,0,sizeof(*rule));rule->id=id;rule->mask=mask;return RS_CANFD_OK;
}
void rs_canfd_receive_filter_accept_data_frames(rs_canfd_receive_filter_t *r){r->data_only=1;}
void rs_canfd_receive_filter_set_minimum_dlc(rs_canfd_receive_filter_t *r,uint8_t d){r->minimum=d;}
rs_canfd_result_t rs_canfd_init(const rs_canfd_config_t *config)
{
    unsigned char i,ch;
    const rs_canfd_channel_config_t *c;
    Check(config->channel_count==CAN_PORT_ENABLED_COUNT,"compact enabled configuration");
    memset(seen,0,sizeof(seen));
    for(i=0;i<config->channel_count;i++){
        c=&config->channels[i];ch=c->channel;
        Check(ch<6 && CanPorts_Enabled(ch) && !seen[ch],"unique enabled channel");
        seen[ch]=1;
        Check(c->fd_mode==(expected_fd?RS_CANFD_CHANNEL_FD_ONLY:RS_CANFD_CHANNEL_CLASSIC_ONLY),"mode");
        Check(c->rx_buffer.count==0,"no global Message Buffer allocation");
        Check(c->rx_fifo_count==1 && c->rx_fifos->fifo==ch,"exclusive FIFO mapping");
        Check(c->rx_fifos->interrupt_enable && c->rx_fifos->interrupt_at_every_frame,"IRQ enabled");
        Check(c->rx_fifos->payload==(expected_fd?RS_CANFD_PAYLOAD_64_BYTES:RS_CANFD_PAYLOAD_8_BYTES),"payload");
        Check(c->receive_filter_count==(expected_raw?2:1) && c->receive_filters[0].id==0x700 &&
            c->receive_filters[0].mask==0x7FF && c->receive_filters[0].data_only,"control filter");
        if(expected_raw){Check(c->receive_filters[1].id==0x720 && c->receive_filters[1].mask==(expected_fd?0x7FF:0x7F8),"raw filter");}
    }
    initialized++;return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_read_rx_fifo(volatile mock_can_t *u,uint8_t ch,rs_canfd_frame_t *out)
{
    (void)u;reads++;Check(CanPorts_Enabled(ch),"never reads disabled FIFO");
    if(positions[ch]>=counts[ch]){return RS_CANFD_ERR_EMPTY;}
    *out=input[ch][positions[ch]++];return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_send_tx_buffer(volatile mock_can_t *u,uint8_t ch,uint8_t b,const rs_canfd_frame_t *f)
{
    Check(CanPorts_Enabled(ch)&&b==0,"local TX buffer0 on requested channel");
    Check(f->frame_mode==(expected_fd?1:0)&&f->brs==(expected_fd?1:0),"TX mode and BRS");
    sent_port=ch;u->tx_status[ch*32U]=tx_fault?1:4;return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_abort_tx_buffer(volatile mock_can_t *u,uint8_t ch,uint8_t b)
{
    (void)u;(void)b;aborted_port=ch;return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_deinit(volatile mock_can_t *u){(void)u;return RS_CANFD_OK;}
static void Fill(unsigned char ch,unsigned char count)
{
    unsigned char i;
    counts[ch]=count;positions[ch]=0;
    for(i=0;i<count;i++){
        memset(&input[ch][i],0,sizeof(input[ch][i]));
        input[ch][i].id=0x700;input[ch][i].length=8;input[ch][i].dlc=8;
        input[ch][i].frame_mode=expected_fd?1:0;input[ch][i].brs=expected_fd?1:0;
        input[ch][i].data[0]=ch;input[ch][i].data[1]=i;
    }
}
int main(int argc,char **argv)
{
    can_port_frame_t frame;
    can_port_diagnostics_t diag;
    unsigned char ch,first,second,data[64];
    unsigned got[6];
    unsigned before,total,loops;
#ifdef TEST_DEVICE_ADAPTER
    boot_frame_t wrapped;
#endif
#ifdef TEST_CLASSICAL_ADAPTER
    device_can_frame_t wrapped;
#endif
    expected_fd=(unsigned char)(argc>1 && argv[1][0]=='1');
    Check(CanPorts_Init(expected_fd,0x700,1,0x720,expected_fd?0x7FF:0x7F8),"initialization");
    Check(initialized==1 && pin_calls==2*CAN_PORT_ENABLED_COUNT,"one init and exact pin count");
    first=CanPorts_First();second=first;memset(data,0,sizeof(data));
    memset(got,0,sizeof(got));
    for(ch=0;ch<6;ch++){if(CanPorts_Enabled(ch)){Fill(ch,2);second=ch;}}
    CanPorts_Irq();total=0;
    while(CanPorts_Read(&frame)){
        Check(frame.channel==frame.data[0] && frame.data[1]==got[frame.channel]++,"per-port order and provenance");total++;
        Check(CanPorts_Send(frame.channel,0x701,frame.data,frame.length)&&sent_port==frame.channel,"reply on original bus");
    }
    Check(total==2*CAN_PORT_ENABLED_COUNT,"all enabled buses delivered");
    before=reads;Check(!CanPorts_Read(&frame)&&reads==before,"main reads RAM only");
    Fill(first,32);if(second!=first){Fill(second,2);}
    before=reads;CanPorts_Irq();Check(reads-before<=16*CAN_PORT_ENABLED_COUNT,"bounded IRQ");
    if(second!=first){Check(CanPorts_Read(&frame),"first bus progress");Check(CanPorts_Read(&frame)&&frame.channel==second,"second bus not starved");}
    while(CanPorts_Read(&frame)){}
    for(loops=0;loops<5;loops++){CanPorts_Irq();while(CanPorts_Read(&frame)){}}
    Check(positions[first]==32,"full batches re-request and drain");
    Fill(first,16);CanPorts_Irq();
    CanPorts_Diagnostics(first,&diag);Check(diag.dropped>=1,"queue full drops only that port");
    if(second!=first){CanPorts_Diagnostics(second,&diag);Check(diag.dropped==0,"other bus queue isolated");}
    while(CanPorts_Read(&frame)){}
    Fill(first,1);input[first][0].brs=expected_fd?0:1;
    CanPorts_Irq();Check(!CanPorts_Read(&frame),"invalid BRS rejected");
    if(expected_fd){
        Fill(first,1);input[first][0].length=64;input[first][0].dlc=15;input[first][0].data[63]=0xA5;
        CanPorts_Irq();Check(CanPorts_Read(&frame)&&frame.data[63]==0xA5,"all FD64 bytes preserved");
    }
    for(ch=0;ch<6;ch++){
        if(CanPorts_Enabled(ch)){Check(CanPorts_Send(ch,0x701,data,8)&&sent_port==ch,"per-channel TX status index");}
        else{Check(!CanPorts_Send(ch,0x701,data,8),"disabled port rejected");}
    }
    tx_fault=1;Check(!CanPorts_Send(second,0x701,data,8)&&aborted_port==second,"timeout aborts only selected TX");
    INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0=1;
    CanPorts_Diagnostics(first,&diag);Check(INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0==1,"snapshot restores mask");
    CanPorts_Stop();Check(!CanPorts_Read(&frame)&&!CanPorts_Send(first,0x701,data,8),"stopped session blocked");
    tx_fault=0;
#if defined(TEST_DEVICE_ADAPTER) || defined(TEST_CLASSICAL_ADAPTER)
#ifdef TEST_DEVICE_ADAPTER
    Check(Device_CanInit(0x700,1U),"device wrapper init");
#else
    expected_raw=0U;
    Check(Device_CanInit(0x700),"transfer wrapper init");
#endif
    for(ch=0;ch<6;ch++){if(CanPorts_Enabled(ch)){Fill(ch,1);}}
#ifdef TEST_DEVICE_ADAPTER
    Device_CanRxIsr();
#else
    rs_canfd_rx_fifo_interrupt();
#endif
    total=0;
    while(Device_CanRead(&wrapped)){
        Check(wrapped.channel==wrapped.data[0],"wrapper preserves source channel");
#ifdef TEST_DEVICE_ADAPTER
        Check(Device_CanSend(0x701,wrapped.data,wrapped.length)&&sent_port==wrapped.channel,"device wrapper replies to source bus");
#else
        Check(Device_CanSend(0x701,wrapped.data)&&sent_port==wrapped.channel,"transfer wrapper replies to source bus");
#endif
        total++;
    }
    Check(total==CAN_PORT_ENABLED_COUNT,"all wrapper buses served");
    Device_CanStop();
#endif
    printf("%s: %u enabled CAN ports, mode=%s; software model only.\n",failures?"FAIL":"PASS",(unsigned)CAN_PORT_ENABLED_COUNT,expected_fd?"FD":"Classical");
    return failures?1:0;
}
