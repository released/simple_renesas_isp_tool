/* Actual device adapter with a simulated peripheral API boundary. */
#include <stdio.h>
#include <string.h>
#include "r_smc_entry.h"
#include "rs_canfd.h"
#include "device_platform.h"
#include "can_boot_config.h"
volatile mock_intc_t INTC1,INTC2;
volatile mock_can_t RCFDC0;
static rs_canfd_frame_t fifo[32];
static unsigned count,at,reads,failures,tx_fault,aborts;
static unsigned long tick;
void Device_CanRxIsr(void);
static void Check(int pass,const char *name){if(!pass){failures++;printf("FAIL: %s\n",name);}}
unsigned long Device_Millis(void){return tick++;}
uint8_t rs_canfd_dlc_to_length(uint8_t d){static const uint8_t len[16]={0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};return d<16?len[d]:0;}
uint8_t rs_canfd_length_to_dlc(uint8_t n){return n<=8?n:(uint8_t)15;}
void R_PORT_SetAltFunc(int p,int pin,int alternate,int dir){Check(p==0 && ((pin==0&&alternate==2&&dir==1)||(pin==1&&alternate==7&&dir==0)),"ECU pin mux");}
rs_canfd_result_t rs_canfd_receive_filter_accept_standard_id_to_rx_fifo(rs_canfd_receive_filter_t *r,uint16_t id,uint16_t mask,uint8_t f){
    Check(f==0 && ((id==0x700&&mask==0x7FF)||(id==0x720&&mask==(RH850_CAN_FD?0x7FF:0x7F8))),"exact control/raw filters");memset(r,0,sizeof(*r));r->id=id;r->mask=mask;return RS_CANFD_OK;
}
void rs_canfd_receive_filter_accept_data_frames(rs_canfd_receive_filter_t *r){r->data_only=1;}
void rs_canfd_receive_filter_set_minimum_dlc(rs_canfd_receive_filter_t *r,uint8_t d){r->minimum=d;}
rs_canfd_result_t rs_canfd_init(const rs_canfd_config_t *c){
    const rs_canfd_channel_config_t *ch;
    ch=c->channels;Check(ch->fd_mode==(RH850_CAN_FD?RS_CANFD_CHANNEL_FD_ONLY:RS_CANFD_CHANNEL_CLASSIC_ONLY),"compiled mode");
    Check(ch->rx_fifo_count==1 && ch->rx_fifos->interrupt_enable && ch->rx_fifos->interrupt_at_every_frame && ch->rx_fifos->payload==(RH850_CAN_FD?RS_CANFD_PAYLOAD_64_BYTES:RS_CANFD_PAYLOAD_8_BYTES),"IRQ FIFO payload");
    Check(ch->receive_filter_count==2 && ch->receive_filters[0].minimum==1,"one-byte ENTER accepted");return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_read_rx_fifo(volatile mock_can_t *u,uint8_t f,rs_canfd_frame_t *out){(void)u;(void)f;reads++;if(at>=count)return RS_CANFD_ERR_EMPTY;*out=fifo[at++];return RS_CANFD_OK;}
rs_canfd_result_t rs_canfd_send_tx_buffer(volatile mock_can_t *u,uint8_t c,uint8_t b,const rs_canfd_frame_t *f){(void)u;(void)c;(void)b;Check(f->frame_mode==(RH850_CAN_FD?1:0)&&f->brs==(RH850_CAN_FD?1:0),"TX mode/BRS");RCFDC0.tx_status=tx_fault?1:4;return RS_CANFD_OK;}
rs_canfd_result_t rs_canfd_abort_tx_buffer(volatile mock_can_t *u,uint8_t c,uint8_t b){(void)u;(void)c;(void)b;aborts++;return RS_CANFD_OK;}
rs_canfd_result_t rs_canfd_deinit(volatile mock_can_t *u){(void)u;return RS_CANFD_OK;}
static void Fill(unsigned n){unsigned i;count=n;at=0;for(i=0;i<n;i++){memset(&fifo[i],0,sizeof(fifo[i]));fifo[i].id=0x700;fifo[i].length=8;fifo[i].dlc=8;fifo[i].frame_mode=RH850_CAN_FD?1:0;fifo[i].brs=RH850_CAN_FD?1:0;fifo[i].data[0]=(uint8_t)i;}}
int main(void){
    boot_frame_t frame;
    unsigned i,before;
    unsigned char data[64]={0};
    Check(Device_CanInit(0x700,1)==1,"init");Fill(16);Device_CanRxIsr();
    Check((Device_CanDiagnostics()&65535UL)==1UL,"queue full drops newest");
    for(i=0;i<15;i++){Check(Device_CanRead(&frame)&&frame.data[0]==i,"queue ordering");}
    before=reads;Check(!Device_CanRead(&frame)&&reads==before,"no polling fallback");
    Fill(32);before=reads;Device_CanRxIsr();Check(reads-before==16 && INTC1.ICRCANGRECC0.BIT.RFRCANGRECC0==1,"bounded IRQ and re-request");
    while(Device_CanRead(&frame)){}Device_CanRxIsr();while(Device_CanRead(&frame)){}Device_CanRxIsr();Check(at==32&&!INTC1.ICRCANGRECC0.BIT.RFRCANGRECC0,"drains subsequent IRQs");
    Fill(2);fifo[0].frame_mode=RH850_CAN_FD?0:1;fifo[1].brs=RH850_CAN_FD?0:1;Device_CanRxIsr();Check(!Device_CanRead(&frame)&&(Device_CanDiagnostics()>>16)==2UL,"wrong mode/BRS rejected");
#if RH850_CAN_FD
    Fill(1);fifo[0].length=64;fifo[0].dlc=15;fifo[0].data[63]=0xA5;Device_CanRxIsr();Check(Device_CanRead(&frame)&&frame.length==64&&frame.data[63]==0xA5,"all64 IRQ bytes preserved");
#endif
    INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0=1;(void)Device_CanDiagnostics();Check(INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0==1,"snapshot preserves mask");
    Check(Device_CanSend(0x701,data,8)==1,"TX_END");tx_fault=1;Check(!Device_CanSend(0x701,data,8)&&aborts==1,"bounded TX abort");
    printf("%s: actual RH850 IRQ/queue adapter; API/register model only.\n",failures?"FAIL":"PASS");return failures?1:0;
}
