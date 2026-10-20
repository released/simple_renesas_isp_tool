#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <vector>
#include "../firmware/common/can_burst.h"
#include "../firmware/common/can_burst.inc"
#include "../firmware/common/diagnostic_clock.h"

/* Compile the production nonblocking dispatcher progression. Only HAL, UART
 * capacity and response sinks are mocked; not CAN timing/electrical behavior. */
namespace {
using st_can_head = struct { struct { uint32_t ID; uint8_t DLC,FDCTR; } Bits; };
struct st_can_tx_frame_t { st_can_head Head; struct { uint8_t DB[64]; } Data; };
struct st_can_rx_frame_t { uint32_t id; uint8_t dlc; uint8_t data[8]; };
using can_length_t=uint8_t;
using can_host_result_t=int;
using e_can_txb_result_t=int;
struct can_host_tx_diagnostic_t {
    uint8_t input_head[8],input_data[8],ram_data[8];
    uint16_t ram_idl,ram_idh,ram_window_before;
    uint8_t ram_dlc_register,ram_fd_control;
};
#define CAN_FDCTR_CLASSICAL 0
#define CAN_FDCTR_FD 6
#define CAN_TXB_TRANSMITTING 0
#define CAN_TXB_END 1
#define CAN_HOST_RESULT_OK 0
#define HOST_LINK_STATUS_OK 0
#define HOST_LINK_STATUS_TIMEOUT 6
#define HOST_LINK_STATUS_CAN_ERROR 7
#define HOST_LINK_CMD_CAN_TX 0x20
#define HOST_LINK_CMD_CAN_TX_WAIT_RX 0x21
#define HOST_LINK_CAN_PHASE_TX 1
#define HOST_LINK_CAN_PHASE_RX 2
#define HOST_LINK_CAN_PHASE_ABORT 3
#define HOST_LINK_CAN_PHASE_GAP 4
#define HOST_LINK_CAN_ABORT_TIMEOUT_MS 100
#define HOST_LINK_MAX_FRAME_SIZE 142
#define HOST_LINK_EVENT_CAN_RX 0x80
struct Transaction {
    uint8_t active=0,command=CB_COMMAND,phase=0,attempts=1,retry_limit=0,rx_matched=0;
    uint16_t timeout_ms=500,phase_started_tick=0,tx_timestamp=0;
    int last_tx_result=0;
    can_length_t rx_length=0;
    st_can_tx_frame_t tx_frame{};
    st_can_rx_frame_t rx_frame{};
    cb_cursor_t burst{};
} g_can_transaction;
struct { uint32_t host_command_errors=0; } g_stats;
uint8_t g_dispatch_payload[128],g_transaction_response[128];
uint16_t tick,uart_free;
std::deque<int> completions;
std::deque<st_can_rx_frame_t> received;
std::vector<st_can_tx_frame_t> transmitted;
unsigned failures=0,finishes,aborts,stops,events;
uint8_t finished_status;
std::vector<uint8_t> finished_payload;
int transmit_result=0,abort_result=0;
void Check(bool value,const char* text) { if (!value) { ++failures; std::cerr<<"FAIL F24 CAN: "<<text<<'\n'; } }
uint16_t get_tick() { return tick; }
uint16_t elapsed_tick_ms(uint16_t now,uint16_t before) { return now>=before?now-before:static_cast<uint16_t>(60000U-before+now); }
uint16_t HostLinkUart_GetTxFree() { return uart_free; }
uint8_t CAN_RxQueuePop(st_can_rx_frame_t* f,can_length_t* length) {
    if (received.empty()) return 0; *f=received.front(); received.pop_front(); *length=f->dlc; return 1;
}
uint8_t rx_frame_matches(const st_can_rx_frame_t* f,can_length_t length) { return f->id==0x701 && length==8; }
uint16_t encode_rx_can_record(const st_can_rx_frame_t* f,can_length_t length,uint8_t* p) {
    std::memset(p,0,12); p[2]=f->dlc; p[3]=length; std::memcpy(p+4,&f->id,4); std::memcpy(p+12,f->data,length); return 12+length;
}
uint8_t send_event(uint8_t,const uint8_t*,uint16_t) { ++events; return 1; }
void increment_u32(uint32_t* p) { ++*p; }
void write_le16(uint8_t* p,uint16_t value) { p[0]=static_cast<uint8_t>(value); p[1]=static_cast<uint8_t>(value>>8U); }
void write_le32(uint8_t* p,uint32_t value) { for(unsigned i=0;i<4;++i)p[i]=static_cast<uint8_t>(value>>(i*8U)); }
uint8_t CAN_HostPollTransmit(int* result,uint16_t* timestamp) {
    if (completions.empty()) return 0; *result=completions.front(); completions.pop_front(); *timestamp=tick; return 1;
}
int CAN_HostTransmit(const st_can_tx_frame_t* f) { transmitted.push_back(*f); return transmit_result; }
void CAN_HostDiscardRxQueue() { received.clear(); }
void CAN_HostGetLastTxDiagnostic(can_host_tx_diagnostic_t* p) { std::memset(p,0,sizeof(*p)); }
int CAN_HostAbortTransmit() { ++aborts; return abort_result; }
int CAN_HostStop(uint32_t*) { ++stops; return 0; }
uint8_t can_result_to_status(int result) { return result?7U:0U; }
void finish_can_transaction(uint8_t status,const uint8_t* p,uint16_t length) {
    ++finishes; finished_status=status; finished_payload.clear();
    if (length) finished_payload.assign(p,p+length); g_can_transaction.active=0;
}
void finish_can_timeout() { finish_can_transaction(HOST_LINK_STATUS_TIMEOUT,nullptr,0); }
#include "../firmware/RL78_F24_Bridge_app/host_link_can_process.inc"

void Prepare(unsigned count=10,unsigned gap=0) {
    g_can_transaction={}; g_can_transaction.active=1; g_can_transaction.phase=HOST_LINK_CAN_PHASE_TX;
    tick=0; uart_free=256; completions.clear(); received.clear(); transmitted.clear();
    finishes=aborts=stops=events=0; finished_payload.clear(); finished_status=0xFF;
    transmit_result=abort_result=0;
    std::vector<uint8_t> p{1,static_cast<uint8_t>(count),244,1,1,7,255,7,8,0,static_cast<uint8_t>(gap),0};
    for (unsigned i=0;i<count;++i) {
        p.push_back(static_cast<uint8_t>(0x700U+i)); p.push_back(7); p.push_back(8);
        for(unsigned k=0;k<8;++k) p.push_back(static_cast<uint8_t>(i*8+k));
    }
    Check(cb_decode(&g_can_transaction.burst,p.data(),static_cast<uint16_t>(p.size()))!=0,"burst fixture valid");
    next_burst_frame(); CAN_HostTransmit(&g_can_transaction.tx_frame);
}
void CompleteToLast() {
    for (unsigned i=1;i<10;++i) {
        completions.push_back(CAN_TXB_END); process_can_transaction();
        Check(g_can_transaction.phase==HOST_LINK_CAN_PHASE_GAP && transmitted.size()==i,"wait for physical completion before advancing");
        process_can_transaction();
        Check(transmitted.size()==i+1U && finishes==0,"no intermediate reply wait / correct progression");
    }
}
}

int TestF24CanProcess() {
    failures=0;
    hl_diagnostic_clock_t clock={};
    for(const unsigned tick_value:{0U,30000U,59999U,1U,30000U,59999U,1U})
        Hl_DiagnosticNow(&clock,static_cast<uint16_t>(tick_value));
    Check(clock.uptime==120001U,"production diagnostics retain transaction durations across multiple60s tick wraps");
    Prepare(); CompleteToLast();
    for(unsigned i=0;i<10;++i) Check(transmitted[i].Head.Bits.ID==0x700U+i &&
        transmitted[i].Head.Bits.DLC==8 && transmitted[i].Data.DB[7]==i*8U+7U,"exact ID/DLC/data reach HAL");
    received.push_back({0x701,8,{0x0C,0,0,0,0x40,0x20,0,0}});
    process_can_transaction();
    Check(!finishes && g_can_transaction.rx_matched,"capture fast ACK before final TX completion");
    completions.push_back(CAN_TXB_END); process_can_transaction();
    Check(finishes==1 && finished_status==0 && finished_payload.size()==20 &&
        finished_payload[12]==0x0C,"one final matching CAN record response");
    Prepare(2,1); completions.push_back(CAN_TXB_END); process_can_transaction(); process_can_transaction();
    Check(transmitted.size()==1,"nonblocking gap holds until deadline");
    tick=1; process_can_transaction(); Check(transmitted.size()==2,"gap expiry advances once");
    Prepare(); completions.push_back(-2); process_can_transaction();
    Check(finishes==1 && finished_status==7 && transmitted.size()==1,"TX failure terminates burst without later frames");
    Prepare(); tick=500; process_can_transaction();
    Check(aborts==1 && g_can_transaction.phase==3 && !finishes,"TX stall requests bounded abort");
    tick=600; process_can_transaction(); Check(stops==1 && finished_status==6,"abort stall forces CAN stop and timeout");
    Prepare(); tick=500; process_can_transaction(); completions.push_back(-2); process_can_transaction();
    Check(finishes==1 && finished_status==6 && stops==0,"completed abort returns timeout");
    Prepare(); CompleteToLast(); completions.push_back(CAN_TXB_END); process_can_transaction();
    Check(g_can_transaction.phase==2,"last TX alone is not a target ACK");
    tick=500; process_can_transaction();
    Check(finishes==1 && finished_status==6 && transmitted.size()==10,"no automatic burst retransmission in generic bridge");
    Prepare(1); g_can_transaction.phase_started_tick=59990; tick=490; process_can_transaction();
    Check(aborts==1,"60000ms clock wrap timeout");
    Prepare(1); uart_free=0; received.push_back({0x701,8,{}}); process_can_transaction();
    Check(received.size()==1 && !g_can_transaction.rx_matched,"UART backpressure does not consume response");
    uart_free=256; received.push_front({0x702,8,{}}); completions.push_back(CAN_TXB_END); process_can_transaction();
    Check(events==1 && finishes==1 && finished_status==0,"unmatched RX event does not steal matching ACK");
    Prepare(1);
    transmitted.clear();
    std::vector<uint8_t> fd{2,3,244,1,1,7,255,7,8,12,0,0};
    for(unsigned i=0;i<3;++i) {
        fd.push_back(i==1U?0x20U:0U);fd.push_back(7U);fd.push_back(i==1U?15U:8U);
        for(unsigned j=0;j<(i==1U?64U:8U);++j)fd.push_back(static_cast<uint8_t>(j));
    }
    Check(cb_decode(&g_can_transaction.burst,fd.data(),static_cast<uint16_t>(fd.size()))!=0,"FD burst fixture");
    next_burst_frame();CAN_HostTransmit(&g_can_transaction.tx_frame);
    for(unsigned i=0;i<2;++i) {
        completions.push_back(CAN_TXB_END);process_can_transaction();process_can_transaction();
    }
    Check(transmitted.size()==3 && transmitted[1].Head.Bits.ID==0x720U &&
        transmitted[1].Head.Bits.DLC==15U && transmitted[1].Data.DB[63]==63U &&
        std::all_of(transmitted.begin(),transmitted.end(),[](const auto& f){return f.Head.Bits.FDCTR==6U;}),
        "actual host progression forwards full FD64 and sets FD+BRS for all three frames");
    received.push_back({0x701,8,{0x0C,0,0,0,0x40,0x20,0,0}});
    completions.push_back(CAN_TXB_END);process_can_transaction();
    Check(finishes==1 && finished_status==0,"FD progression waits for final ACK once");
    std::cout<<"Actual F24 nonblocking CAN progression / fast ACK / timeout / abort / gap / wrap HAL model: "<<(failures?"FAIL":"PASS")<<'\n';
    return static_cast<int>(failures);
}
