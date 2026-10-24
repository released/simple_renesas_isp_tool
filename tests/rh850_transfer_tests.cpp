// Production PC client against the independently compiled RH850 C protocol.
#include "f13_transfer_profile.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#ifndef RH850_CAN_FD
#define RH850_CAN_FD 0
#endif
extern "C" {
#include "can_transfer_protocol.h"
}
using namespace firmware_upgrade;
namespace {
unsigned failures;
void Check(bool value,const char* message) {
    if(!value){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
class Link final : public host_link::Transport {
    std::deque<uint8_t> rx;
public:
    ct_protocol_t state{};
    bool bad_crc=false;
    bool replay=false;
    bool IsOpen() const override{return true;}
    bool WriteAll(const uint8_t* bytes,std::size_t size,uint32_t,std::wstring*) override {
        host_link::Frame request,response;
        if(host_link::DecodeFrame(bytes,size,&request)!=host_link::DecodeResult::Ok)return false;
        Check(request.command==0x21U && (request.payload.size()==36U || request.payload.size()==92U),"generic CAN TX_WAIT_RX bounds");
        if(request.payload.size()!=36U && request.payload.size()!=92U)return false;
        const auto* p=request.payload.data();
#if RH850_CAN_FD
        Check(p[16]==0 && p[17]==12 && (p[18]==8 || p[18]==15) && p[20]==0x10 && p[21]==7,"FD+BRS STD710 request");
        uint8_t reply[64]{};
        const auto count=CT_ProcessFrame(&state,p+28,p[19],reply);
#else
        Check(p[16]==0 && p[17]==0 && p[18]==8 && p[19]==8 && p[20]==0x10 && p[21]==7,
              "Classical standard 710 DLC8 request");
        uint8_t reply[8]{};
        CT_Process(&state,p+28,reply);
        const uint8_t count=8;
#endif
        if(replay){
            uint8_t again[8]{};CT_Process(&state,p+28,again);
            Check(!std::memcmp(reply,again,8),"byte-identical duplicate replay");
        }
        if(bad_crc)reply[7]^=1;
        ++state.stats[1];
        response.type=host_link::FrameType::Response;
        response.sequence=request.sequence;response.command=request.command;
        response.payload={0,RH850_CAN_FD?uint8_t{12}:uint8_t{0},static_cast<uint8_t>(count==64?15:8),count,0x11,7,0,0,0,0,0,0};
        response.payload.insert(response.payload.end(),reply,reply+count);
        std::vector<uint8_t> encoded;host_link::EncodeFrame(response,&encoded);
        rx.insert(rx.end(),encoded.begin(),encoded.end());return true;
    }
    bool ReadSome(std::vector<uint8_t>* bytes,std::size_t max,uint32_t,std::wstring*) override {
        bytes->clear();while(!rx.empty() && bytes->size()<max){bytes->push_back(rx.front());rx.pop_front();}
        return true;
    }
};
void Direct(ct_protocol_t& state,uint8_t sequence,uint8_t command,uint8_t a,uint8_t b,uint8_t c,
            uint8_t* reply) {
    uint8_t request[8]={0xA5,1,sequence,command,a,b,c,0};
    request[7]=CT_Crc8(request,7);CT_Process(&state,request,reply);
    Check(reply[0]==0x5A && reply[2]==sequence && reply[7]==CT_Crc8(reply,7),"reply CRC/envelope");
}
}
int main() {
    Link transport;CT_Init(&transport.state);
    host_link::Client link;can_host::Client can;f13_transfer::Client client;
    link.SetTransport(&transport);can.SetHostLinkClient(&link);client.SetCanClient(&can);client.SetCanFd(RH850_CAN_FD!=0);
    std::wstring error;f13_transfer::DeviceInfo info;
    Check(client.Discover(&info,&error) && info.target_id==0x8501U &&
          info.part_number=="R7F70168" && info.capabilities==(RH850_CAN_FD?0x7FU:0x3FU),"RH850 Discover seven pages");
#if RH850_CAN_FD
    std::array<uint8_t,59> pattern{};
    for(unsigned n=0;n<3;++n){for(unsigned i=0;i<59;++i)pattern[i]=static_cast<uint8_t>(n==0?i:n==1?255-i:(i%2?0x55:0xAA));Check(client.EchoFd64(pattern,&error),"FD+BRS64 / all59 payload / CRC8");}
#endif
    Check(client.Ping({0x12,0x34,0x56},&error),"PING");
    std::array<uint8_t,3> out{};
    Check(client.Echo({0,0xA5,0xFF},&out,&error) && out==std::array<uint8_t,3>{0,0xA5,0xFF},"ECHO");
    Check(client.PatternTest({0x5A,0xC3,0x81},&out,&error) && out==std::array<uint8_t,3>{0xFF,0,3},"PATTERN");
    for(uint8_t i=0;i<12;++i){uint16_t value=0;Check(client.GetStat(i,&value,&error),"all12 GET_STATS");}
    uint16_t value=0;
    Check(!client.GetStat(12,&value,&error),"out-of-range statistic rejected");
    transport.replay=true;
    Check(client.Ping({0,1,2},&error),"lost response exact replay");
    transport.replay=false;
    Check(transport.state.stats[6]>0,"duplicate counted");
    Check(client.ClearStats(&error),"CLEAR_STATS key");
    Check(transport.state.stats[0]==0 && transport.state.stats[6]==0,"clear resets counters");
    transport.bad_crc=true;
    Check(!client.Ping({1,2,3},&error),"corrupt response rejected by production PC parser");
    ct_protocol_t state{};uint8_t reply[8]{};
    ct_protocol_t port1{},port4{};uint8_t other[8]{},cached1[8]{};
    CT_Init(&port1);CT_Init(&port4);
    Direct(port1,1,3,1,2,3,reply);std::memcpy(cached1,reply,8);
    Direct(port4,1,3,4,5,6,other);
    Check(reply[4]==1 && other[4]==4,"same sequence has independent per-bus payloads");
    Direct(port1,1,3,1,2,3,reply);
    Check(!std::memcmp(cached1,reply,8) && port1.stats[6]==1 && port4.stats[6]==0,
          "per-bus duplicate cache and counters remain isolated");
    Direct(state,1,0x44,0,0,0,reply);Check(reply[3]==0x7F && reply[5]==6,"unknown command");
    Direct(state,1,2,0,0,0,reply);Check(reply[3]==0x7F && reply[5]==4,"same sequence changed content rejected");
    Direct(state,2,1,7,0,0,reply);Check(reply[3]==0x7F && reply[5]==7,"bad INFO page");
    Direct(state,3,6,0,0,0,reply);Check(reply[3]==0x7F && reply[5]==7,"bad clear key");
    Direct(state,0,2,0,0,0,reply);Check(reply[3]==0x7F && reply[5]==4,"sequence zero rejected");
    uint8_t request[8]={0xA5,1,4,2,0,0,0,0};
    request[7]=CT_Crc8(request,7)^1;CT_Process(&state,request,reply);
    Check(reply[3]==0x7F && reply[5]==5 && state.stats[5]==1,"bad request CRC");
    request[0]=0;request[7]=CT_Crc8(request,7);CT_Process(&state,request,reply);
    Check(reply[3]==0x7F && reply[5]==2,"bad magic");
    request[0]=0xA5;request[1]=2;request[7]=CT_Crc8(request,7);CT_Process(&state,request,reply);
    Check(reply[3]==0x7F && reply[5]==3,"bad protocol version");
    state.stats[0]=65535;Direct(state,5,2,0,0,0,reply);Check(state.stats[0]==65535,"counter saturation");
    for(unsigned i=0;i<600;++i)Direct(state,static_cast<uint8_t>(i%255+1),2,1,2,3,reply);
#if RH850_CAN_FD
    ct_protocol_t fdstate{};uint8_t full[64]={0xA5,1,1,7},fdreply[64]{},cached[64]{};
    for(unsigned i=4;i<63;++i)full[i]=static_cast<uint8_t>(i);
    full[63]=CT_Crc8(full,63);
    Check(CT_ProcessFrame(&fdstate,full,64,fdreply)==64,"actual FD64 decoder");
    std::memcpy(cached,fdreply,64);
    Check(CT_ProcessFrame(&fdstate,full,64,fdreply)==64&&!std::memcmp(cached,fdreply,64),"full64-byte replay cache");
    full[62]^=1;full[63]=CT_Crc8(full,63);
    Check(CT_ProcessFrame(&fdstate,full,64,fdreply)==8&&fdreply[5]==4,"same sequence changed FD byte62 rejected");
    uint8_t shortframe[8]={0xA5,1,1,2,0,0,0,0};shortframe[7]=CT_Crc8(shortframe,7);
    Check(CT_ProcessFrame(&fdstate,shortframe,8,fdreply)==8&&fdreply[5]==4,"FD64 to8 sequence collision");
    full[2]=2;full[63]=CT_Crc8(full,63)^1;
    Check(CT_ProcessFrame(&fdstate,full,64,fdreply)==8&&fdreply[5]==5,"FD byte63 CRC rejected");
    Check(CT_ProcessFrame(&fdstate,full,12,fdreply)==0,"unsupported protocol length rejected");
#endif
    if(!failures)std::cout<<"PASS RH850 C90 protocol / production PC client; hardware NOT RUN\n";
    return failures?1:0;
}
