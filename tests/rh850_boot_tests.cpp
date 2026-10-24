// Actual C90 Boot engine, production PC codecs/client and modeled Flash.
#include "f13_boot_profile.h"
#include "f13_boot_update.h"
#include "firmware_update_profile.h"
#include "../firmware/common/can_burst.h"
#include "../firmware/common/can_burst.inc"
#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
extern "C" {
#include "boot_engine.h"
#include "flash_geometry.h"
#include "can_boot_config.h"
}
using namespace firmware_upgrade;
namespace {
unsigned failures;
void Check(bool value,const char* text){if(!value){++failures;std::cerr<<"FAIL: "<<text<<'\n';}}
std::vector<uint8_t> flash(IMAGE_CODE_FLASH_SIZE,255);
std::set<unsigned long> programmed;
unsigned erases,writes;
bool fail_write,fail_erase;
unsigned char Read(unsigned long at,unsigned char* out,unsigned short n){
    if(at>=flash.size() || n>flash.size()-at)return 0;
    std::memcpy(out,flash.data()+at,n);return 1;
}
unsigned char Erase(unsigned long at){
    unsigned long block;
    Check(Device_FlashBlockAllowed(at,&block)!=0 && block>=8 &&
        block<8+IMAGE_APPLICATION_CAPACITY/IMAGE_ERASE_BLOCK_SIZE,"App-only exact erase geometry");
    if(fail_erase)return 0;
    ++erases;std::fill(flash.begin()+at,flash.begin()+at+0x8000,uint8_t{255});
    for(auto i=programmed.begin();i!=programmed.end();)if(*i>=at && *i<at+0x8000)i=programmed.erase(i);else ++i;
    return 1;
}
unsigned char Write(unsigned long at,const unsigned char* p){
    Check(Device_FlashPageAllowed(at)!=0 && !programmed.count(at),"one256-byte ECC program per erased page");
    if(fail_write)return 0;
    programmed.insert(at);++writes;
    for(unsigned i=0;i<256;++i){Check(flash[at+i]==255,"no repeated programming");flash[at+i]=p[i];}
    return 1;
}
const boot_io_t io={Read,Erase,Write};
boot_engine_t state;
std::vector<uint8_t> Process(uint32_t id,const uint8_t* data,uint8_t n,uint8_t channel=1){
    boot_frame_t f{};f.id=id;f.length=n;f.channel=channel;std::memcpy(f.data,data,n);
    uint8_t reply[8]{},action=0;
    const auto length=Boot_Process(&state,&f,reply,&action);
    return {reply,reply+length};
}
std::vector<uint8_t> Control(std::initializer_list<uint8_t> p,uint8_t channel=1){std::vector<uint8_t> v(p);return Process(0x700,v.data(),static_cast<uint8_t>(v.size()),channel);}
class Link final:public host_link::Transport {
    std::deque<uint8_t> rx;
public:
    uint8_t channel=1;
    bool lost=false,corrupt=false,conflict=false,missing=false,padding=false;
    bool injected=false,closed=false;
    bool IsOpen()const override{return !closed;}
    bool WriteAll(const uint8_t* p,std::size_t n,uint32_t,std::wstring*)override{
        if(closed)return false;
        host_link::Frame r,s;
        if(host_link::DecodeFrame(p,n,&r)!=host_link::DecodeResult::Ok)return false;
        s.type=host_link::FrameType::Response;s.command=r.command;s.sequence=r.sequence;
        std::vector<uint8_t> reply;
        if(r.command==0x22){
            cb_cursor_t cursor{};Check(cb_decode(&cursor,r.payload.data(),static_cast<uint16_t>(r.payload.size()))!=0,"production host burst decode");
            uint16_t id;uint8_t dlc,data[64];unsigned index=0;
            while(cursor.remaining){
                Check(cb_next(&cursor,&id,&dlc,data)!=0,"bounded burst record");
                const auto count=static_cast<uint8_t>(dlc<=8?dlc:64);
                if(id>=0x720 && id<=0x727 && (!injected || (padding && state.block_length<64))){
                    if(corrupt)data[0]^=1;
                    if(missing){++index;continue;}
                    if(padding && state.block_length<64)data[state.block_length]=0;
                    reply=Process(id,data,count,channel);
                    if(conflict){data[0]^=1;reply=Process(id,data,count,channel);}
                }else reply=Process(id,data,count,channel);
                ++index;
            }
            Check(index>=3,"BEGIN/data/COMMIT");
            if(lost && !injected && !reply.empty() && !reply[1])s.status=host_link::Status::Timeout;
            injected=true;
        }else{
            Check(r.command==0x21 && r.payload.size()>=29,"generic TX_WAIT_RX");
            reply=Process(0x700,r.payload.data()+28,r.payload[19],channel);
        }
        if(s.status==host_link::Status::Ok){
            const auto count=static_cast<uint8_t>(reply.size());
            s.payload={0,RH850_CAN_FD?uint8_t{12}:uint8_t{0},count,count,1,7,0,0,0,0,0,0};
            s.payload.insert(s.payload.end(),reply.begin(),reply.end());
        }
        std::vector<uint8_t> bytes;host_link::EncodeFrame(s,&bytes);rx.insert(rx.end(),bytes.begin(),bytes.end());return true;
    }
    bool ReadSome(std::vector<uint8_t>* out,std::size_t max,uint32_t,std::wstring*)override{
        out->clear();while(!rx.empty() && out->size()<max){out->push_back(rx.front());rx.pop_front();}return !closed;
    }
};
void Reset(){std::fill(flash.begin(),flash.end(),uint8_t{255});programmed.clear();erases=writes=0;fail_write=fail_erase=false;Boot_Init(&state,&io,1);}
void Recrc(std::vector<uint8_t>& image){Device_Write32(image.data()+image.size()-4,f13_boot::Crc32(image.data(),image.size()-4));}
void Install(const std::vector<uint8_t>& image){std::copy(image.begin(),image.end(),flash.begin()+0x10000);Boot_Init(&state,&io,1);}
struct Session {
    Link transport;host_link::Client host;can_host::Client can;f13_boot::Client device;std::wstring error;
    explicit Session(uint8_t channel=1){transport.channel=channel;host.SetTransport(&transport);can.SetHostLinkClient(&host);device.SetCanClient(&can);device.SetCanFd(RH850_CAN_FD!=0);}
    bool Probe(){f13_boot::TargetInfo info;return device.GetTargetInfo(&info,&error) && info.target_id==0x8501 && info.mcu_part==L"R7F701684" && device.SetTargetLayout(info.application_start,info.application_capacity,info.erase_block_size,&error);}
    bool Start(const std::vector<uint8_t>& image,bool force=false){f13_boot::ImageInfo info;f13_boot::Ack ack;return f13_boot::ParseUpdateImage(image,&info,&error) && (info.force_old_version=force,true) && device.StageMetadata(image,[]{return false;},&error) && device.Start(info,&ack,&error);}
};
void Run(const std::vector<uint8_t>& image,unsigned fault,uint8_t channel=1){
    Reset();Session s(channel);s.transport.lost=fault==1;s.transport.corrupt=fault==2;s.transport.conflict=fault==3;s.transport.missing=fault==4;
    Check(s.Probe(),"actual Boot live INFO / production client");
    f13_boot::UpdateCallbacks callbacks;f13_boot::UpdateReport report;
    const bool ok=f13_boot::RunUpdate(&s.device,image,callbacks,&report,&s.error);
    if(!ok)std::wcerr<<s.error<<'\n';
    Check(ok,"actual packaged image update");
    Check(std::equal(image.begin(),image.end(),flash.begin()+0x10000),"exact programmed BIN");
    Check(writes==(image.size()+255)/256 && erases==(image.size()+32767)/32768,"page aggregation / replay / erase counts");
    device_image_info_t im{};Check(Device_ImageValidate(Read,&im)!=0 && im.size==image.size(),"actual full CRC/entry/exception/vector validator");
}
}
int main(int argc,char** argv){
    static_assert(sizeof(unsigned long)==4,"32-bit C metadata word");
    if(argc!=2)return 2;
    std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> image(std::istreambuf_iterator<char>(f),{});
    if(image.empty())return 2;
    for(unsigned fault=0;fault<5;++fault)Run(image,fault);
    Run(image,0,4);
    Reset();
    Check(Control({8,0,0,0,0,0,0,0})[1]==2,"metadata requires a claimed bus");
    Check(Control({10})[1]==0 && state.owner_valid && state.owner_channel==1,"CAN1 claims metadata session");
    Check(Control({1},4)[1]==0 && Control({7,0},4)[1]==0 && state.owner_channel==1,"CAN4 discovery stays read-only");
    for(uint8_t command:{uint8_t{2},uint8_t{4},uint8_t{5},uint8_t{8},uint8_t{9},uint8_t{10},uint8_t{12},uint8_t{14}}){
        const auto reply=Control({command,0,0,0,0,0,0,0},4);
        Check(reply.size()==8 && reply[1]==2 && reply[2]==1,"non-owner command rejected with owner channel");
    }
    uint8_t raw[64]{};
    Check(Process(0x720,raw,RH850_CAN_FD?64:8,4).empty() && Control({11,0,0,0,0,0,0,0},4).empty(),"non-owner raw/BEGIN ignored");
    Check(!state.staged && !state.active && !erases && !writes,"other bus cannot mutate staging or Flash");
    Boot_ReleasePort(&state);
    Check(!state.owner_valid && !erases && !writes,"pre-START lease release is RAM-only");
    Check(Control({10},4)[1]==0 && state.owner_channel==4,"CAN4 claims after release");
    Check(Control({5},4)[1]==0 && !state.owner_valid,"owner ABORT releases bus");
    {Session owner(1);Check(owner.Probe() && owner.Start(image),"CAN1 starts update");
     const auto next=state.next;
     Check(Control({10},4)[1]==2 && state.active && state.owner_channel==1 && state.next==next,"active update cannot be taken over");
     Check(Control({5},1)[1]==0 && !state.owner_valid && state.ever_started,"ABORT keeps post-START launch inhibition");
     Session nextOwner(4);Check(nextOwner.Probe() && nextOwner.Start(image),"CAN4 starts after explicit ABORT");}
    auto maximum=image;maximum.resize(IMAGE_APPLICATION_CAPACITY,255);Device_Write32(maximum.data()+0x118,IMAGE_APPLICATION_CAPACITY);Recrc(maximum);
    Run(maximum,1); // Includes the16-bit END word-count wrap above256KiB.
    unsigned long block;
    Check(!Device_FlashBlockAllowed(0,&block) && !Device_FlashBlockAllowed(0x12000,&block) && !Device_FlashBlockAllowed(IMAGE_CODE_FLASH_SIZE,&block),"protected/misaligned/out-of-range erase rejected");
    Check(!Device_FlashPageAllowed(0xFFFF) && !Device_FlashPageAllowed(0x10004) && Device_FlashPageAllowed(0x7FF00),"page bounds");
    for(unsigned off:{0U,0x100U,0x174U,0x200U,0x400U}){
        Reset();auto bad=image;bad[off]^=1;if(off==0x174)bad[off]=2;Recrc(bad);Install(bad);device_image_info_t im{};
        Check(!Device_ImageValidate(Read,&im),"CRC-correct invalid entry/metadata/policy/EBASE/IRQ rejected");
    }
    for(unsigned word:{0U,3U,7U,9U,25U,26U,27U,29U,32U,63U}){
        Reset();Session s;Check(s.Probe(),"negative probe");auto bad=image;
        if(word==29)Device_Write32(bad.data()+0x100+word*4,2);else bad[0x100+word*4]^=0x80;
        Recrc(bad);Check(!s.Start(bad),"invalid staged contract rejected before erase");Check(!erases&&!writes,"pre-erase safety");
    }
    auto current=image;Device_Write32(current.data()+0x110,0x20000);Device_Write32(current.data()+0x174,1);Recrc(current);
    Reset();Install(current);{Session s;Check(s.Probe()&&!s.Start(image),"valid enabled installed policy rejects older");Check(!erases&&!writes,"version reject before erase");Check(s.Start(image,true),"transaction-local force bypasses ordering only");}
    Reset();Install(current);{Session s;Check(s.Probe()&&s.Start(current),"same version accepted");}
    auto newer=current;Device_Write32(newer.data()+0x110,0x30000);Recrc(newer);
    Reset();Install(current);{Session s;Check(s.Probe()&&s.Start(newer),"new version accepted");}
    Device_Write32(current.data()+0x174,0);Recrc(current);
    Reset();Install(current);{Session s;Check(s.Probe()&&s.Start(image),"disabled installed policy allows older");}
    for(unsigned cut:{0U,64U,192U,256U,512U,1024U}){
        Reset();Session s;Check(s.Probe()&&s.Start(image),"power model start");
        for(unsigned off=0;off<cut;off+=64){f13_boot::Ack ack;Check(s.device.SendBlock(off,{image.begin()+off,image.begin()+off+64},&ack,&s.error),"power model block");}
        Boot_Init(&state,&io,1);device_image_info_t im{};Check(!Device_ImageValidate(Read,&im),"interrupted empty/partial image does not launch");
        Session recovery;Check(recovery.Probe()&&recovery.Start(image),"invalid App recovery has no retained version floor");
    }
    for(unsigned fault=0;fault<2;++fault){
        Reset();Session s;Check(s.Probe()&&s.Start(image),"Flash failure start");fail_erase=fault==0;fail_write=fault==1;
        bool failed=false;for(unsigned off=0;off<256;off+=64){f13_boot::Ack ack;if(!s.device.SendBlock(off,{image.begin()+off,image.begin()+off+64},&ack,&s.error)){failed=true;break;}}
        Check(failed&&!state.active,"Flash failure stops session before END");
    }
    Reset();{Session s;Check(s.Probe()&&s.Start(image),"abort start");f13_boot::Ack ack;Check(s.device.Abort(&ack,&s.error)&&!state.active&&!state.sealed,"ABORT clears RAM transaction");}
    Reset();{Session s;Check(s.Probe(),"disconnect probe");s.transport.closed=true;Check(!s.Start(image)&&!erases&&!writes,"disconnect before staging never erases");}
#if RH850_CAN_FD
    Reset();{Session s;Check(s.Probe()&&s.Start(image),"FD padding start");s.transport.padding=true;
        bool rejected=false;
        for(unsigned off=0;off<image.size();off+=64){f13_boot::Ack ack;const auto n=(std::min)(std::size_t{64},image.size()-off);if(!s.device.SendBlock(off,{image.begin()+off,image.begin()+off+n},&ack,&s.error)){rejected=true;break;}}
        Check(rejected,"non-FF FD tail padding rejected");}
#endif
    std::cout<<(failures?"FAIL":"PASS")<<": actual RH850 C90 engine + production PC, page/replay/CRC/IRQ/policy/fault models. Hardware NOT RUN.\n";
    return failures?1:0;
}
