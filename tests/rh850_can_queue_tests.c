/* Real RH850 IRQ/queue/TX adapter, simulated register/API boundary.
 * This model does not execute the vendor driver or validate bus timing.
 */
#include <stdio.h>
#include <string.h>
#include "r_smc_entry.h"
#include "rs_canfd.h"
#include "rh850_can.h"
volatile mock_intc_t INTC1,INTC2;
volatile mock_can_t RCFDC0;
static unsigned failures,pins,init_calls,reads,read_at,aborts,tx_fault;
static uint32_t tick;
static rs_canfd_frame_t fifo[32];
static unsigned count;
void rs_canfd_rx_fifo_interrupt(void);
static void Check(int pass,const char *name)
{
    if (!pass) { failures++; printf("FAIL: %s\n",name); }
}
void R_PORT_SetAltFunc(int port,int pin,int alternate,int direction)
{
    Check(port==0 && ((pin==0 && alternate==2 && direction==1) ||
          (pin==1 && alternate==7 && direction==0)),"ECU CAN0 pin mux"); pins++;
}
uint32_t Device_Millis(void) { return tick++; }
rs_canfd_result_t rs_canfd_receive_filter_accept_standard_id_to_rx_fifo(
    rs_canfd_receive_filter_t *r,uint16_t id,uint16_t mask,uint8_t f)
{
    Check(id==0x710 && mask==0x7FF && f==0,"exact STD710 filter");
    memset(r,0,sizeof(*r));r->id=id;r->mask=mask;return RS_CANFD_OK;
}
void rs_canfd_receive_filter_accept_data_frames(rs_canfd_receive_filter_t *r){r->data_only=1;}
void rs_canfd_receive_filter_set_minimum_dlc(rs_canfd_receive_filter_t *r,uint8_t d){r->minimum=d;}
rs_canfd_result_t rs_canfd_init(const rs_canfd_config_t *c)
{
    const rs_canfd_channel_config_t *ch;
    ch=c->channels;
    Check(c->unit==&RCFDC0 && c->channel_count==1 && ch->channel==0 && ch->fd_mode==2,"one Classical CAN0 channel");
    Check(ch->receive_filter_count==1 && ch->receive_filters->data_only && ch->receive_filters->minimum==8,"data-only DLC8 filter");
    Check(ch->rx_buffer.count==0 && ch->rx_fifo_count==1 && ch->rx_fifos->enable &&
        ch->rx_fifos->interrupt_enable && ch->rx_fifos->interrupt_at_every_frame,"FIFO-only per-frame interrupt");
    Check(ch->bit_timing.nominal_brp==0 && ch->bit_timing.nominal_tseg1==58 &&
        ch->bit_timing.nominal_tseg2==19,"40MHz /80tq =500K");init_calls++;return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_read_rx_fifo(volatile mock_can_t *u,uint8_t f,rs_canfd_frame_t *v)
{
    Check(u==&RCFDC0 && f==0,"FIFO0 read"); reads++;
    if(read_at>=count)return RS_CANFD_ERR_EMPTY;
    *v=fifo[read_at++];return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_send_tx_buffer(volatile mock_can_t *u,uint8_t ch,uint8_t b,const rs_canfd_frame_t *f)
{
    Check(u==&RCFDC0 && ch==0 && b==0 && f->id==0x711 && f->dlc==8 && f->length==8 &&
          !f->frame_mode && !f->brs && !f->id_format && !f->frame_type,"Classical STD711 TX");
    RCFDC0.tx_status=tx_fault?1:4;return RS_CANFD_OK;
}
rs_canfd_result_t rs_canfd_abort_tx_buffer(volatile mock_can_t *u,uint8_t c,uint8_t b)
{(void)u;(void)c;(void)b;aborts++;return RS_CANFD_OK;}
rs_canfd_result_t rs_canfd_deinit(volatile mock_can_t *u){(void)u;return RS_CANFD_OK;}
static void Fill(unsigned n)
{
    unsigned i;
    count=n;read_at=0;
    for(i=0;i<n;i++){
        memset(&fifo[i],0,sizeof(fifo[i]));fifo[i].id=0x710;
        fifo[i].length=8;fifo[i].dlc=8;fifo[i].data[0]=(uint8_t)i;
    }
}
int main(void)
{
    device_can_frame_t f;
    device_can_diagnostics_t d;
    unsigned i,before;
    uint8_t bytes[8]={0};
    Check(Device_CanInit(0x710)==1 && init_calls==1 && pins==2,"init");
    Check(INTC1.ICRCANGRECC0.BIT.TBRCANGRECC0==1 && !INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0,"RX table IRQ unmasked");
    Fill(16);rs_canfd_rx_fifo_interrupt();
    Device_CanDiagnostics(&d);Check(d.irq==1 && d.dropped==1 && d.peak==15,"full queue drops newest");
    for(i=0;i<15;i++)Check(Device_CanRead(&f)==1 && f.data[0]==i,"FIFO ordered dequeue");
    before=reads;Check(!Device_CanRead(&f) && before==reads,"main has no hardware polling fallback");
    for(i=0;i<80;i++){
        Fill(3);rs_canfd_rx_fifo_interrupt();
        Check(Device_CanRead(&f) && f.data[0]==0,"queue wrap first");
        Check(Device_CanRead(&f) && f.data[0]==1,"queue wrap second");
        Check(Device_CanRead(&f) && f.data[0]==2 && !Device_CanRead(&f),"queue wrap third");
    }
    Fill(6);fifo[0].frame_mode=1;fifo[1].brs=1;fifo[2].frame_type=1;
    fifo[3].id_format=1;fifo[4].dlc=7;fifo[5].length=7;rs_canfd_rx_fifo_interrupt();
    Device_CanDiagnostics(&d);Check(d.invalid==6 && !Device_CanRead(&f),"invalid mode/type/length rejected");
    Fill(32);before=reads;rs_canfd_rx_fifo_interrupt();Check(reads-before==16,"ISR drain bounded16");
    Check(INTC1.ICRCANGRECC0.BIT.RFRCANGRECC0==1,"full batch re-requests IRQ without polling");
    while(Device_CanRead(&f)){}
    rs_canfd_rx_fifo_interrupt();
    Check(read_at==32 && INTC1.ICRCANGRECC0.BIT.RFRCANGRECC0==1,"second bounded batch");
    while(Device_CanRead(&f)){}
    rs_canfd_rx_fifo_interrupt();
    Check(INTC1.ICRCANGRECC0.BIT.RFRCANGRECC0==0,"empty pass clears request");
    INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0=1;Device_CanDiagnostics(&d);
    Check(INTC1.ICRCANGRECC0.BIT.MKRCANGRECC0==1,"snapshot restores already-masked IRQ");
    Check(Device_CanSend(0x711,bytes)==1,"TX_END success");
    tx_fault=1;Check(Device_CanSend(0x711,bytes)==0 && aborts==1,"TX timeout bounded abort");
    Device_CanStop();Check(!Device_CanSend(0x711,bytes),"stop rejects TX");
    if(!failures)puts("PASS actual RH850 IRQ/queue adapter; vendor registers/electrical timing modeled");
    return failures?1:0;
}
