/* Production UART ISR/queue/send/diagnostic bodies with register boundaries. */
#include "isp_protocol.h"
#include <stdio.h>
#include <string.h>
static unsigned char rx_register,tx_register,STIF1,SRMK1,send_ok;
static unsigned short error_register,clear_register;
static unsigned char *TxAddress(void)
{ if(send_ok){STIF1=1U;}return &tx_register; }
#define SDR11L rx_register
#define RXD1 rx_register
#define SDR10L (*TxAddress())
#define TXD1 (*TxAddress())
#define SSR11 error_register
#define SSR03 error_register
#define SIR11 clear_register
#define SIR03 clear_register
#define ISP_NOP() ((void)0)
#define BSP_NOP() ((void)0)
#include "actual_rl78_uart.inc"
#define CHECK(x) do {if(!(x)){printf("FAIL UART adapter line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    unsigned short at,capacity;
    unsigned char byte,diagnostics[24],value;
    capacity=(unsigned short)(sizeof(g_rx)-1U);
    for(at=0U;at<capacity+1U;++at){rx_register=(unsigned char)at;IspUartRxIsr();}
    SRMK1=1U;IspUartDiagnostics(diagnostics);CHECK(SRMK1==1U);
    CHECK(IspRead32(diagnostics+8U)==1UL&&IspRead32(diagnostics+16U)==capacity&&IspRead32(diagnostics+20U)==capacity);
    SRMK1=0U;IspUartDiagnostics(diagnostics);CHECK(SRMK1==0U);
    for(at=0U;at<capacity;++at){CHECK(IspUartGet(&byte)&&byte==(unsigned char)at);}
    CHECK(!IspUartGet(&byte));
    for(at=0U;at<1000U;++at){rx_register=(unsigned char)at;IspUartRxIsr();CHECK(IspUartGet(&byte)&&byte==(unsigned char)at);}
    error_register=7U;IspUartRxIsr();CHECK(clear_register==7U&&!IspUartGet(&byte));
    IspUartDiagnostics(diagnostics);CHECK(IspRead32(diagnostics+4U)==1UL&&IspRead32(diagnostics+20U)==0UL);
    send_ok=0U;value=0x55U;CHECK(!IspUartSend(&value,1U));
    send_ok=1U;CHECK(IspUartSend(&value,1U)&&tx_register==0x55U);
    IspUartDiagnostics(diagnostics);CHECK(IspRead32(diagnostics+12U)==1UL);
    g_tx_errors=65535U;send_ok=0U;CHECK(!IspUartSend(&value,1U));
    IspUartDiagnostics(diagnostics);CHECK(IspRead32(diagnostics+12U)==65535UL);
    IspTimerIsr();CHECK(IspTick()==1U);
    puts("PASS actual RL78 UART ISR/queue/drop/error/TX timeout/diagnostic snapshot and mask restoration;not physical timing.");
    return 0;
}
