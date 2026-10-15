#define HOST_LINK_UART_TEST
#include "../firmware/f24_board/host_link_uart.c"
#include <stdio.h>
volatile uint16_t SDR00, SDR01;
static uint16_t mock_tick, mock_tx_count;
static int failures;
uint16_t get_tick(void) { return mock_tick; }
void R_Config_UART0_Stop(void) { }
void R_Config_UART0_Start(void) { }
MD_STATUS R_Config_UART0_Send(uint8_t *data, uint16_t count)
{ (void)data; mock_tx_count=count; return MD_OK; }
static void check(int value, const char *name)
{ if (!value) { printf("FAIL UART: %s\n",name); ++failures; } }
int TestF24Uart(void)
{
    uint32_t nominal, actual;
    uint8_t ack[18], byte;
    unsigned i;
    failures=0; mock_tick=0U; HostLinkUart_Init();
    check(HostLinkUart_PrepareBaud(500000UL)==3U,"unadvertised speed rejected");
    check(HostLinkUart_ConfirmBaud(115200UL)==10U,"confirm without SET rejected");
    check(HostLinkUart_PrepareBaud(460800UL)==0U,"prepare460800");
    HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==115200UL,"never switch before queued ACK");
    for (i=0U;i<sizeof(ack);++i) ack[i]=(uint8_t)i;
    check(HostLinkUart_Write(ack,sizeof(ack)),"queue ACK"); HostLinkUart_ArmBaud();
    HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==115200UL && mock_tx_count==sizeof(ack),"ACK uses old speed");
    HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==115200UL,"wait for physical TX completion");
    HostLinkUart_TxCompleteFromIsr(); HostLinkUart_RxByteFromIsr(0xAAU);
    HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==460800UL && actual==454545UL && SDR00==0x2A00U && SDR01==SDR00,"20MHz divisor460800");
    check(HostLinkUart_Read(&byte,1U)==0U && HostLinkUart_TakeRateChange(),"drop old-speed partial bytes");
    check(HostLinkUart_PrepareBaud(921600UL)==1U,"one switch at a time");
    check(HostLinkUart_ConfirmBaud(921600UL)==10U,"wrong confirm rejected");
    mock_tick=1999U; HostLinkUart_Process();
    check(HostLinkUart_BaudPending()==3U,"confirmation deadline bounded");
    mock_tick=2000U; HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==115200UL && !HostLinkUart_BaudPending(),"lost ACK/confirmation returns bootstrap");
    mock_tick=59500U;
    check(!HostLinkUart_PrepareBaud(921600UL),"prepare921600"); HostLinkUart_ArmBaud(); HostLinkUart_Process();
    HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==921600UL && actual==909090UL && SDR00==0x1400U,"20MHz divisor921600");
    mock_tick=1499U; HostLinkUart_Process(); check(HostLinkUart_BaudPending()==3U,"60000 tick wrap before deadline");
    mock_tick=1500U; HostLinkUart_Process(); check(!HostLinkUart_BaudPending(),"60000 tick wrap timeout");
    check(!HostLinkUart_PrepareBaud(921600UL),"reconnect high"); HostLinkUart_ArmBaud(); HostLinkUart_Process();
    check(!HostLinkUart_ConfirmBaud(921600UL),"new-rate confirm commits");
    mock_tick=10000U; HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==921600UL,"confirmed idle link does not silently change");
    check(!HostLinkUart_PrepareBaud(115200UL),"explicit close restore"); HostLinkUart_ArmBaud(); HostLinkUart_Process();
    check(!HostLinkUart_ConfirmBaud(115200UL),"bootstrap confirm");
    check(!HostLinkUart_PrepareBaud(460800UL),"prepare with stuck TX");
    check(HostLinkUart_Write(ack,sizeof(ack)),"queue stuck ACK"); HostLinkUart_ArmBaud(); HostLinkUart_Process();
    mock_tick=12000U; HostLinkUart_Process(); HostLinkUart_BaudInfo(&nominal,&actual);
    check(nominal==115200UL && !HostLinkUart_BaudPending() && HostLinkUart_IsTxIdle(),"stuck TX cannot prevent bounded recovery");
    for (i=0U;i<255U;++i) HostLinkUart_RxByteFromIsr((uint8_t)i);
    HostLinkUart_RxByteFromIsr(0U);
    check(g_uart_stats.rx_overflow_count==1U,"bounded RX pressure records overflow");
    for (i=0U;i<255U;++i) { check(HostLinkUart_Read(&byte,1U)==1U && byte==(uint8_t)i,"RX ordering"); }
    printf("F24 real UART baud/ACK/rollback/wrap/queue tests: %s\n",failures?"FAIL":"PASS");
    return failures;
}
