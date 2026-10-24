#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
extern "C" {
#include "isp_i2c_mailbox.h"
}
namespace {
struct Reg { unsigned long UINT32=0; unsigned short UINT16=0; };
struct Irq { unsigned short UINT16=0; struct { unsigned MKRIIC1TEI=0,MKRLIN31UR0=0; } BIT; };
struct { Irq ICRIIC1TEI,ICRIIC1EE,ICRLIN31UR0; } INTC2;
struct { Reg CR2,MR3,DRR,DRT,SR2,SAR0,IER; } RIIC1;
struct { Reg LURDR,LBRP01,LUTDR; unsigned char LEST=0,LST=0,LWBR=0; } RLN31;
struct { unsigned long CKSC_ILINS_ACT=1UL,CKSC_ILIND_ACT=1UL; } CLKCTL;
unsigned short tick=0,step=0;
unsigned creates=0,starts=0,stops=0,saves=0,restores=0;
void Check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
unsigned short IspTick(){const auto value=tick;tick=static_cast<unsigned short>(tick+step);return value;}
unsigned long Device_IrqSave(){++saves;return 0x20UL;}
void Device_IrqRestore(unsigned long psw){Check(psw==0x20UL,"IRQ mask restored exactly");++restores;}
void __syncp(){}
void R_Config_RIIC1_Create(){++creates;INTC2.ICRIIC1EE.UINT16=0x00CFU;}
void R_Config_RIIC1_Start(){++starts;}
void R_Config_RIIC1_Stop(){++stops;}
void R_Config_UART1_Create(){++creates;RLN31.LBRP01.UINT16=0x1CU;RLN31.LWBR=0x52U;}
void R_Config_UART1_Start(){++starts;}
void R_Config_UART1_Stop(){++stops;}
// Register bit values come from the selected vendor headers, included by the
// runner. The production adapter body is included unchanged except includes/
// target interrupt pragmas. This model does not emulate electrical timing.
#include "vendor_hal_bits.h"
#include "actual_hal.inc"
#if defined(TEST_I2C)
void Event(unsigned long flags){RIIC1.SR2.UINT32=flags;IspI2cEventIsr();}
void Rx(unsigned char byte){
    RIIC1.DRR.UINT32=byte;RIIC1.SR2.UINT32|=_RIIC_RECEIVE_COMPLETE;IspI2cRxIsr();
    RIIC1.SR2.UINT32&=~_RIIC_RECEIVE_COMPLETE; // Hardware clears RDRF on DRR read.
}
void WriteRequest(){
    Event(_RIIC_START_CONDITION_DETECTED);RIIC1.CR2.UINT32=0;Rx(0x40U);
    for(unsigned at=0;at<14U;++at)Rx(static_cast<unsigned char>(at));
    Event(_RIIC_STOP_CONDITION_DETECTED);
}
unsigned char reply[14]={};
void Publish(){
    WriteRequest();unsigned char byte=0;for(unsigned at=0;at<14U;++at)Check(IspBusGet(&byte)&&byte==at,"STOP-delimited bytes");
    Check(IspBusSend(reply,14U)!=0U,"publish one immutable reply");
}
void ReadLoad(unsigned count){
    Event(_RIIC_START_CONDITION_DETECTED);RIIC1.CR2.UINT32=_RIIC_TRANSMIT_MODE;
    for(unsigned at=0;at<count;++at){
        RIIC1.SR2.UINT32=_RIIC_TRANSMIT_DATA_EMPTY;IspI2cTxIsr();
        if(!at)Check(RIIC1.DRT.UINT32==ISP_I2C_READY,"snapshot ready byte");
        else if(at<=14U)Check(RIIC1.DRT.UINT32==reply[at-1U],"reply bytes unchanged");
        else Check(RIIC1.DRT.UINT32==0xFFU,"fixed window padding");
    }
}
void Test(){
    Check(IspBusInit()&&creates==1U&&starts==1U,"RIIC init");
    Check(RIIC1.SAR0.UINT32==0x40UL&&INTC2.ICRIIC1TEI.BIT.MKRIIC1TEI==1U,"7-bit20 and TEI mask");
    Check(INTC2.ICRIIC1EE.UINT16==0xCEU,"event priority preserves table bits");
    for(unsigned at=0;at<14U;++at)reply[at]=static_cast<unsigned char>(0x80U+at);
    Publish();ReadLoad(111U); // 110 clocked bytes plus one DRT-prefetched byte.
    Event(_RIIC_NACK_DETECTED);
    Check((RIIC1.SR2.UINT32&_RIIC_NACK_DETECTED)&&!(RIIC1.IER.UINT32&_RIIC_NACK_RECEPTION_INT_ENABLE),"NACK remains latched,request masked until STOP");
    Check(!IspBusReplyComplete(),"never complete before STOP");
    Event(_RIIC_STOP_CONDITION_DETECTED);
    Check(!IspBusReplyComplete()&&g_box.reply_ready,"short prefetched read cannot consume reply");
    ReadLoad(112U);Event(_RIIC_NACK_DETECTED);Check(!IspBusReplyComplete(),"full read awaits STOP");
    Event(_RIIC_STOP_CONDITION_DETECTED);
    Check(IspBusReplyComplete()&&!g_box.busy&&(RIIC1.IER.UINT32&_RIIC_NACK_RECEPTION_INT_ENABLE),"full window NACK/STOP consumes and re-enables");
    Publish();ReadLoad(111U);Event(_RIIC_NACK_DETECTED|_RIIC_TRANSMIT_DATA_EMPTY);
    Event(_RIIC_STOP_CONDITION_DETECTED);Check(IspBusReplyComplete(),"empty DRT proves111 bytes left DRT");
    Publish();ReadLoad(111U);Event(_RIIC_NACK_DETECTED|_RIIC_STOP_CONDITION_DETECTED);
    Check(!IspBusReplyComplete()&&g_box.reply_ready,"coalesced STOP with ambiguous prefetch retains reply");
    ReadLoad(112U);Event(_RIIC_NACK_DETECTED|_RIIC_STOP_CONDITION_DETECTED);Check(IspBusReplyComplete(),"coalesced STOP proven full reply");
    // Event IRQ can win over the pending final receive IRQ.
    Event(_RIIC_START_CONDITION_DETECTED);RIIC1.CR2.UINT32=0;Rx(0x40U);
    for(unsigned at=0;at<13U;++at)Rx(static_cast<unsigned char>(at));
    RIIC1.DRR.UINT32=13U;Event(_RIIC_STOP_CONDITION_DETECTED|_RIIC_RECEIVE_COMPLETE);
    RIIC1.SR2.UINT32&=~_RIIC_RECEIVE_COMPLETE;IspI2cRxIsr();
    Check(g_box.ready&&g_box.receive_count==14U,"last RX drained once before STOP publication");
    tick=static_cast<unsigned short>(tick+2000U);IspBusPoll();
    Check(!g_box.busy&&!g_box.reply_done,"lease expiry cannot manufacture completed reply");
    Publish();ReadLoad(112U);Event(_RIIC_ARBITRATION_LOST|_RIIC_NACK_DETECTED|_RIIC_STOP_CONDITION_DETECTED);
    Check(!IspBusReplyComplete()&&g_bus_errors==1UL,"bus error invalidates snapshot and prevents reset");
    Check(saves==restores&&saves>0U,"mailbox publication/poll preserve interrupt state");
    IspBusStop();Check(stops==1U,"RIIC stop");
}
#else
void Test(){
    CLKCTL.CKSC_ILIND_ACT=2UL;Check(!IspBusInit(),"changed RLIN clock divider rejected");
    CLKCTL.CKSC_ILIND_ACT=1UL;CLKCTL.CKSC_ILINS_ACT=2UL;Check(!IspBusInit(),"changed RLIN clock source rejected");
    CLKCTL.CKSC_ILINS_ACT=1UL;
    Check(IspBusInit()&&creates==1U&&starts==1U,"UART init");
    Check(INTC2.ICRLIN31UR0.BIT.MKRLIN31UR0==1U,"TX IRQ masked for bounded polled transmit");
    unsigned char byte=0;
    Check(!IspBusGet(&byte),"empty queue");
    for(unsigned at=0;at<512U;++at){RLN31.LURDR.UINT16=static_cast<unsigned short>(at&255U);IspUartRxIsr();}
    Check(g_rx_drops==1UL,"511 usable slots drop newest");
    for(unsigned at=0;at<511U;++at)Check(IspBusGet(&byte)&&byte==(at&255U),"FIFO ordering after full");
    Check(!IspBusGet(&byte),"queue drained");
    for(unsigned at=0;at<1500U;++at){RLN31.LURDR.UINT16=static_cast<unsigned short>(at&255U);IspUartRxIsr();Check(IspBusGet(&byte)&&byte==(at&255U),"multiple ring wraps");}
    RLN31.LEST=0xFFU;IspUartErrorIsr();Check(g_rx_errors==1UL&&!(RLN31.LEST&_UART_CLEAR_ERROR_FLAG),"error ISR clears flag without enqueuing");
    Check(!IspBusGet(&byte),"error byte not accepted");
    const unsigned char data[3]={1U,2U,3U};
    Check(!IspBusSend(nullptr,1U)&&!IspBusSend(data,0U)&&!IspBusSend(data,111U),"TX argument bounds");
    Check(IspBusSend(data,3U)&&RLN31.LUTDR.UINT16==3U,"idle bounded transmit");
    RLN31.LST=_UART_TRANSMISSION_OPERATED;step=1000U;
    Check(!IspBusSend(data,3U),"busy transmission times out without reset");
    unsigned char diagnostics[ISP_DIAGNOSTIC_BYTES]={};IspBusDiagnostics(diagnostics);
    Check(IspRead32(diagnostics+4U)==1UL&&IspRead32(diagnostics+8U)==1UL&&
        IspRead32(diagnostics+12U)==1UL&&IspRead32(diagnostics+16U)==511UL,"coherent device diagnostic counters");
    IspBusStop();Check(stops==1U,"UART stop");
}
#endif
}
int main(){try{Test();std::cout<<"PASS actual RH850 IRQ/queue/mailbox adapter transitions;not hardware timing.\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
