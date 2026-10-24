#include "isp_rh850.h"
#include "Config_UART1.h"

/* UART1 uses a bounded SPSC byte queue. UART0 remains diagnostic text.
 * The ISR has no parser, formatted output, Flash call or transmission wait.
 */
static volatile unsigned short g_rx_head,g_rx_tail;
static unsigned char g_rx[512];
static volatile unsigned long g_rx_errors,g_rx_drops;
#if !defined(ISP_DEVICE_BOOT)
static unsigned long g_tx_errors;
static volatile unsigned short g_rx_peak;
void IspBusDiagnostics(unsigned char *bytes)
{
    unsigned long saved;
    saved=Device_IrqSave();
    bytes[0]=1U;bytes[1]=1U;bytes[2]=0x0FU;bytes[3]=0U;
    IspWrite32(bytes+4U,g_rx_errors);IspWrite32(bytes+8U,g_rx_drops);
    IspWrite32(bytes+12U,g_tx_errors);IspWrite32(bytes+16U,g_rx_peak);
    IspWrite32(bytes+20U,(unsigned short)((g_rx_head-g_rx_tail)&511U));
    Device_IrqRestore(saved);
}
#endif
void R_Config_UART1_Create_UserInit(void){}
#pragma interrupt IspUartRxIsr(enable=false,fpu=true,callt=false)
void IspUartRxIsr(void)
{
    unsigned short next;
    unsigned char byte;
    byte=(unsigned char)RLN31.LURDR.UINT16;
    next=(unsigned short)((g_rx_head+1U)&511U);
    if(next==g_rx_tail){if(g_rx_drops!=0xFFFFFFFFUL){++g_rx_drops;}return;}
    g_rx[g_rx_head]=byte;__syncp();g_rx_head=next;
#if !defined(ISP_DEVICE_BOOT)
    next=(unsigned short)((g_rx_head-g_rx_tail)&511U);
    if(next>g_rx_peak){g_rx_peak=next;}
#endif
}
#pragma interrupt IspUartErrorIsr(enable=false,fpu=true,callt=false)
void IspUartErrorIsr(void)
{
    volatile unsigned short discarded;
    discarded=RLN31.LURDR.UINT16;(void)discarded;
    RLN31.LEST&=(unsigned char)~_UART_CLEAR_ERROR_FLAG;
    if(g_rx_errors!=0xFFFFFFFFUL){++g_rx_errors;}
}
unsigned char IspBusInit(void)
{
    unsigned long actual,divisor,error;
    g_rx_head=g_rx_tail=0U;g_rx_errors=g_rx_drops=0UL;
    /* SDK r_cg_cgc.h: RLIN source PPLLCLK2=1, divider1=1. The owned EVM
     * clock projection fixes PPLLCLK2 at40MHz; reject a changed UART mux.
     */
    if(CLKCTL.CKSC_ILINS_ACT!=1UL||CLKCTL.CKSC_ILIND_ACT!=1UL){return 0U;}
    R_Config_UART1_Create();R_Config_UART1_Start();
    INTC2.ICRLIN31UR0.BIT.MKRLIN31UR0=1U;
    /* EVM RLIN communication source is PPLLCLK2/1=40MHz. LWBR NSPB
     * selects samples/bit; LPRS selects a power-of-two prescaler (manual22).
     * Compare actual generated registers, not a hard-coded115200 divisor.
     */
    divisor=(((unsigned long)RLN31.LWBR>>4)+1UL)*
        (1UL<<((RLN31.LWBR>>1)&7U))*((unsigned long)RLN31.LBRP01.UINT16+1UL);
    actual=40000000UL/divisor;
    error=(actual>ISP_BUS_RATE)?actual-ISP_BUS_RATE:ISP_BUS_RATE-actual;
    return (unsigned char)(error*100UL<=ISP_BUS_RATE*2UL);
}
void IspBusStop(void){R_Config_UART1_Stop();}
unsigned char IspBusGet(unsigned char *byte)
{
    if(g_rx_tail==g_rx_head){return 0U;}
    *byte=g_rx[g_rx_tail];__syncp();g_rx_tail=(unsigned short)((g_rx_tail+1U)&511U);return 1U;
}
unsigned char IspBusSend(const unsigned char *data,unsigned short size)
{
    unsigned short at,start;
    unsigned long guard,limit;
    if(!data||!size||size>ISP_FRAME_MAX){return 0U;}
    limit=((unsigned long)size*10000UL+ISP_BUS_RATE-1UL)/ISP_BUS_RATE+250UL;
    start=IspTick();
    for(at=0U;at<size;++at){
        guard=1000000UL;
        while((RLN31.LST&_UART_TRANSMISSION_OPERATED)&&guard){--guard;}
        if(!guard||(unsigned short)(IspTick()-start)>=limit){
#if !defined(ISP_DEVICE_BOOT)
            if(g_tx_errors!=0xFFFFFFFFUL){++g_tx_errors;}
#endif
            return 0U;
        }
        RLN31.LUTDR.UINT16=data[at];
    }
    guard=1000000UL;
    while((RLN31.LST&_UART_TRANSMISSION_OPERATED)&&guard){--guard;}
    if(!guard){
#if !defined(ISP_DEVICE_BOOT)
        if(g_tx_errors!=0xFFFFFFFFUL){++g_tx_errors;}
#endif
        return 0U;
    }
    return 1U;
}
void IspBusPoll(void){}
unsigned char IspBusReplyComplete(void){return 1U;}
