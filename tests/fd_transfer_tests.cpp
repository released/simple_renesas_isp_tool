// Real device C protocol and PC client; CAN registers/electrical timing are modeled.
#include "f13_transfer_profile.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
extern "C" {
#include "can_transfer_config.h"
#include "can_transfer_protocol.h"
#include "transfer_diagnostics.h"
}
using namespace firmware_upgrade;
namespace {
unsigned failures, tick;
can_frame_t pending{}, sent{};
bool available=false, transmitted=false;
unsigned snapshots=0;
Can_RtnType tx_result=CAN_RTN_TX_END;
void Check(bool ok,const char* text) {
    if(!ok) {++failures;std::cerr<<"FAIL FD Transfer: "<<text<<'\n';}
}
bool Inject(const can_frame_t& frame) {
    // Finish the previous modeled transmission before delivering another frame.
    available=false;CAN_TransferProtocol_Process();
    transmitted=false;pending=frame;available=true;++tick;
    CAN_TransferProtocol_Process();
    return transmitted;
}
enum class Fault { None, NoBrs, Classical, WrongId, WrongSequence, BadCrc, BadEcho, Truncated, Retry, Timeout, BadTimeout };
class Link final : public host_link::Transport {
    std::deque<uint8_t> rx;
public:
    Fault fault=Fault::None;
    std::vector<uint8_t> timeout_payload{2,0,0,0,2,0,0,0};
    unsigned fd64=0, retries=0;
    bool IsOpen() const override {return true;}
    bool WriteAll(const uint8_t* bytes,std::size_t size,uint32_t,std::wstring*) override {
        host_link::Frame request, response;
        if(host_link::DecodeFrame(bytes,size,&request)!=host_link::DecodeResult::Ok)return false;
        Check(request.command==0x21U && request.payload.size()>=36U,"generic TX_WAIT_RX command");
        Check(request.payload[12]==12U && request.payload[13]==15U,"FD+BRS strict reply filter");
        can_host::Frame frame;
        const std::vector<uint8_t> record(request.payload.begin()+16,request.payload.end());
        Check(record.size()>=12U && record[0]==0U && record[1]==12U &&
              record.size()==12U+record[3],"encoded TX record bounds and flags");
        frame.fd=(record[1]&4U)!=0;frame.bit_rate_switch=(record[1]&8U)!=0;
        frame.dlc=record[2];frame.id=static_cast<uint32_t>(record[4]|(record[5]<<8U));
        frame.data.assign(record.begin()+12,record.end());
        Check(frame.fd && frame.bit_rate_switch && frame.id==0x710U,"request FD+BRS/ID");
        can_frame_t hardware{};
        hardware.IDL=static_cast<uint16_t>(frame.id);
        hardware.DLC=static_cast<uint8_t>(frame.data.size());
        std::copy(frame.data.begin(),frame.data.end(),hardware.DB);
        bool got=Inject(hardware);
        if(frame.dlc==15U) {
            ++fd64;
            Check(frame.data.size()==64U && request.payload[3]==15U,"wire DLC15/64bytes");
            if(fault==Fault::Retry) {
                const auto original=sent;
                Check(got && Inject(hardware) && !std::memcmp(original.DB,sent.DB,64),"lost ACK identical request replays exact64-byte response");
                ++retries;
            }
        }
        response.type=host_link::FrameType::Response;
        response.command=request.command;response.sequence=request.sequence;
        if(fault==Fault::Timeout || fault==Fault::BadTimeout) {
            response.status=host_link::Status::Timeout;
            response.payload=timeout_payload;
            if(fault==Fault::BadTimeout)response.payload.resize(4);
        }
        else if(!got)response.status=host_link::Status::Timeout;
        else {
            can_host::Frame reply;
            reply.id=fault==Fault::WrongId?0x712U:0x711U;
            reply.fd=fault!=Fault::Classical;
            reply.bit_rate_switch=fault!=Fault::NoBrs && fault!=Fault::Classical;
            reply.dlc=sent.DLC==64?15U:sent.DLC;
            reply.data.assign(sent.DB,sent.DB+sent.DLC);
            if(fault==Fault::WrongSequence)reply.data[2]^=1;
            if(fault==Fault::BadEcho)reply.data[4]^=1;
            if(fault==Fault::WrongSequence || fault==Fault::BadEcho)
                reply.data.back()=f13_transfer::Crc8Atm(reply.data.data(),reply.data.size()-1);
            if(fault==Fault::BadCrc)reply.data.back()^=1;
            if(fault==Fault::Truncated) {
                reply.dlc=8;reply.data.resize(8);
            }
            if(fault==Fault::Classical && reply.data.size()==64) {
                reply.dlc=8;reply.data.resize(8);
            }
            response.payload={0,static_cast<uint8_t>((reply.fd?4U:0U)|(reply.bit_rate_switch?8U:0U)),
                reply.dlc,static_cast<uint8_t>(reply.data.size()),static_cast<uint8_t>(reply.id),
                static_cast<uint8_t>(reply.id>>8U),0,0,0,0,0,0};
            response.payload.insert(response.payload.end(),reply.data.begin(),reply.data.end());
        }
        std::vector<uint8_t> encoded;host_link::EncodeFrame(response,&encoded);
        rx.insert(rx.end(),encoded.begin(),encoded.end());return true;
    }
    bool ReadSome(std::vector<uint8_t>* bytes,std::size_t max,uint32_t,std::wstring*) override {
        bytes->clear();while(!rx.empty() && bytes->size()<max) {bytes->push_back(rx.front());rx.pop_front();}return true;
    }
};
}
extern "C" {
unsigned int get_tick(void) {return tick;}
Can_RtnType Device_CanRead(can_frame_t* frame) {
    if(!available)return CAN_RTN_BUFFER_EMPTY;*frame=pending;available=false;return CAN_RTN_OK;
}
Can_RtnType Device_CanStartTx(const can_frame_t* frame) {sent=*frame;transmitted=true;return CAN_RTN_OK;}
Can_RtnType Device_CanTxResult(void) {return tx_result;}
Can_RtnType Device_CanAbortTx(void) {return CAN_RTN_OK;}
void Device_CanGetRuntimeDiagnostics(transfer_can_runtime_t* state) {
    ++snapshots;std::memset(state,0,sizeof(*state));
    state->rx_buffer_config=0x0701U;state->rx_new_data=0U;
    state->last_rx_before=0U;state->last_rx_after=0U;
    state->rx_irq_enable=1U;state->rx_queue_peak=1U;
}
}
int main() {
    CAN_TransferProtocol_Init(CAN_RTN_OK);
    Link transport;host_link::Client link;can_host::Client can;f13_transfer::Client client;
    link.SetTransport(&transport);can.SetHostLinkClient(&link);client.SetCanClient(&can);client.SetCanFd(true);
    std::wstring error;f13_transfer::DeviceInfo info;
    Check(client.Discover(&info,&error) && info.target_id==0xF24BU &&
          (info.capabilities&CT_FD64_CAPABILITY),"real FD Discover pages/capability");
    std::array<uint8_t,3> output{};
    Check(client.Ping({0x12,0x34,0x56},&error),"FD PING");
    Check(client.Echo({0,0xA5,0xFF},&output,&error),"FD ECHO8");
    Check(client.PatternTest({0x5A,0xC3,0x81},&output,&error),"FD pattern");
    std::array<uint8_t,CT_FD64_PAYLOAD> data{};
    for(unsigned pattern=0;pattern<3;++pattern) {
        for(unsigned i=0;i<data.size();++i)data[i]=static_cast<uint8_t>(pattern==0?i:pattern==1?255U-i:(i%2?0x55U:0xAAU));
        Check(client.EchoFd64(data,&error),"FD64 all59 echo bytes / CRC");
    }
    transport.fault=Fault::Retry;
    Check(client.EchoFd64(data,&error) && transport.retries==1,"FD64 retry/replay");
    transport.fault=Fault::None;
    for(uint8_t index=0;index<12;++index) {uint16_t value=0;Check(client.GetStat(index,&value,&error),"GET_STATS");}
    transport.fault=Fault::Timeout;
    uint16_t value=0;
    Check(!client.GetStat(3,&value,&error),"timeout response remains failure");
    Check(error.find(L"Host returned TIMEOUT for command 0x21.")==0 &&
          error.find(L"attempts=2, last_tx_result=2 (TX_END)")!=std::wstring::npos,
          "exact host status retained and eight-byte timeout detail decoded");
    transport.fault=Fault::BadTimeout;
    Check(!client.GetStat(3,&value,&error) && error.find(L"invalid diagnostic payload")!=std::wstring::npos,
          "truncated timeout diagnostic not interpreted as TX success");
    transport.fault=Fault::Timeout;
    for(unsigned field:{0U,1U,2U,3U}) {
        transport.timeout_payload={2,0,0,0,2,0,0,0};
        transport.timeout_payload[field]=field==0U?5U:1U;
        Check(!client.GetStat(3,&value,&error) && error.find(L"invalid diagnostic payload")!=std::wstring::npos,
              "invalid attempt count/reserved timeout fields rejected");
    }
    transport.timeout_payload={2,0,0,0,77,0,0,0};
    Check(!client.GetStat(3,&value,&error) && error.find(L"77 (UNKNOWN)")!=std::wstring::npos,
          "unknown raw TX result stays unknown");
    transport.timeout_payload={2,0,0,0,255,255,255,255};
    Check(!client.GetStat(3,&value,&error) && error.find(L"-1 (ARG_ERROR)")!=std::wstring::npos,
          "signed argument error remains signed");
    for(const auto fault:{Fault::NoBrs,Fault::Classical,Fault::WrongId,Fault::WrongSequence,Fault::BadCrc,Fault::BadEcho,Fault::Truncated}) {
        transport.fault=fault;
        Check(!client.EchoFd64(data,&error),"PC rejects bad FD64 mode/ID/sequence/CRC/echo/length");
    }
    transport.fault=Fault::None;
    for(unsigned i=0;i<260;++i)Check(client.Ping({1,2,3},&error),"sequence wrap");
    can_frame_t raw{};raw.IDL=0x710;raw.DLC=64;
    raw.DB[0]=0xA5;raw.DB[1]=1;raw.DB[2]=2;raw.DB[3]=CT_FD64_COMMAND;
    raw.DB[63]=f13_transfer::Crc8Atm(raw.DB,63);
    Check(Inject(raw),"raw FD64 valid");
    Check(Inject(raw),"raw identical replay");
    raw.DB[4]^=1;raw.DB[63]=f13_transfer::Crc8Atm(raw.DB,63);
    Check(!Inject(raw),"conflicting same sequence dropped");
    raw.DB[2]=3;raw.DB[63]^=1;
    Check(!Inject(raw),"bad CRC dropped");
    for(unsigned field=0;field<4;++field) {
        auto bad=raw;bad.DB[field]=0;bad.DB[63]=f13_transfer::Crc8Atm(bad.DB,63);
        Check(!Inject(bad),"bad magic/version/sequence/command dropped");
    }
    raw.DLC=12;Check(!Inject(raw),"unsupported byte length dropped");
    raw.DLC=64;raw.IDE=1;Check(!Inject(raw),"extended rejected");
    raw.IDE=0;raw.RTR=1;Check(!Inject(raw),"RTR rejected");
    raw.RTR=0;raw.IDL=0x712;Check(!Inject(raw),"wrong request ID rejected");
    client.SetCanFd(false);Check(!client.EchoFd64(data,&error),"Classical cannot invoke FD64");
    Check(client.IsCanFd()==false,"mode reset retained");
    available=false;CAN_TransferProtocol_Process();
    const auto before_idle=snapshots;
    for(unsigned i=0;i<12;++i) {tick+=1001U;CAN_TransferProtocol_Process();}
#if (CAN_TRANSFER_RUNTIME_DIAGNOSTICS_ENABLE != 0U)
    Check(snapshots-before_idle==3,"idle snapshots bounded to three until more traffic");
    Check(snapshots>3,"checkpoint snapshots run across more than16 requests");
    CAN_TransferProtocol_Init(CAN_RTN_OK);
    raw={};raw.IDL=0x710;raw.DLC=8;
    raw.DB[0]=0xA5;raw.DB[1]=1;raw.DB[2]=1;raw.DB[3]=2;
    raw.DB[7]=f13_transfer::Crc8Atm(raw.DB,7);
    tx_result=CAN_RTN_TRANSMITTING;
    Check(Inject(raw),"TX timeout diagnostic setup");
    tick+=101;CAN_TransferProtocol_Process();
    tick+=101;CAN_TransferProtocol_Process();
#else
    Check(snapshots==before_idle && snapshots==0,"trace-off defaults runtime snapshots off");
#endif
    std::cout<<"Production PC + actual FD Transfer C protocol: "<<(failures?"FAIL":"PASS")<<'\n';
    return static_cast<int>(failures);
}
