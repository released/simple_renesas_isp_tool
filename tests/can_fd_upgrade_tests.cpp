// Production PC client + host burst codec + device RAM helpers.
// Flash, reset and physical CAN are modeled, not hardware evidence.
#include "f13_boot_profile.h"
#include "f13_boot_update.h"
#include "firmware_update_profile.h"
#include "../firmware/common/can_burst.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include "../firmware/common/can_burst.inc"
#define __far
#define F13_CRC_HOST_TEST
#include "can_boot_transport.h"
#include "sw_crc32.c"
#include "can_boot_metadata.inc"
#include "can_boot_block.inc"
using namespace firmware_upgrade;
namespace {
unsigned failures;
void Check(bool ok,const char* text) {
    if(!ok) { ++failures;std::cerr<<"FAIL FD: "<<text<<'\n'; }
}
uint32_t Read(const uint8_t* p) { return image_policy::Read32(p); }
void Put(std::vector<uint8_t>& p,uint32_t value) {
    for(unsigned i=0;i<4;++i)p.push_back(static_cast<uint8_t>(value>>(8U*i)));
}
enum class Fault { None, LostAck, Corrupt, Missing, Conflict, BadPadding, WrongMode, ClassicalReply, WrongId, WrongDlc, Disconnect };
class Link final : public host_link::Transport {
    std::deque<uint8_t> rx;
    bool injected=false,started=false;
    unsigned next=0;
public:
    std::vector<uint8_t> flash;
    Fault fault=Fault::None;
    unsigned writes=0,blocks=0,ends=0;
    bool opened=true;
    explicit Link(unsigned size):flash(size,0xFFU) { MetadataReset();g_block_bad=1U; }
    bool IsOpen() const override { return opened; }
    std::vector<uint8_t> Ack(uint8_t cmd,uint8_t status,uint16_t sequence,uint32_t value) {
        std::vector<uint8_t> p{cmd,status,static_cast<uint8_t>(sequence),static_cast<uint8_t>(sequence>>8U)};
        Put(p,value);return p;
    }
    std::vector<uint8_t> Control(const uint8_t* p) {
        const auto cmd=p[0];
        if(cmd==CAN_BOOT_CMD_ENTER)return {cmd,0};
        if(cmd==CAN_BOOT_CMD_INFO) {
            const auto page=p[1];
            if(page==0x1BU)return Ack(cmd,0,static_cast<uint16_t>(page|0x100U),0x30701U);
            const uint32_t values[]={
                0x0605F24BU,IMAGE_BOOT_VERSION,IMAGE_BOOT_BUILD,0xFFFFFFFFU,0xFFFFFFFFU,
                IMAGE_APPLICATION_START,IMAGE_APPLICATION_CAPACITY,IMAGE_CODE_FLASH_SIZE,IMAGE_RESERVED_SIZE,
                IMAGE_ERASE_BLOCK_SIZE,0x01010002U,4U,2U,0x700U,0x701U,
                IMAGE_MCU_PART_0,IMAGE_MCU_PART_1,IMAGE_MCU_PART_2,IMAGE_MCU_PART_3,
                IMAGE_MCU_FAMILY_0,IMAGE_MCU_FAMILY_1,IMAGE_MCU_FAMILY_2,0xFFFFFFFFU,
                IMAGE_MINIMUM_IMAGE_SIZE,IMAGE_POLICY_WORD,IMAGE_IRQ_ABI,CAN_BOOT_BLOCK_INFO};
            Check(page<27U || page==0x1DU,"bounded INFO");
            return Ack(cmd,0,static_cast<uint16_t>(page|0x200U),page<27U?values[page]:0U);
        }
        if(cmd==CAN_BOOT_CMD_METADATA_RESET) { MetadataReset();return Ack(cmd,0,0,256); }
        if(cmd==CAN_BOOT_CMD_METADATA) {
            const auto status=MetadataStage(static_cast<uint16_t>(p[1]|(p[2]<<8U)),p+4);
            return Ack(cmd,status,g_metadata_staged,256);
        }
        if(cmd==CAN_BOOT_CMD_SEAL) {
            const auto status=MetadataSeal(Read(p+4));return Ack(cmd,status,g_metadata_staged,256);
        }
        if(cmd==CAN_BOOT_CMD_START) {
            Check(g_metadata_sealed && Read(p+4)==flash.size(),"sealed metadata before START");
            started=true;next=0;return Ack(cmd,0,0,0x2000);
        }
        if(cmd==CAN_BOOT_CMD_ABORT) {started=false;return Ack(cmd,0,0,0x2000+next);}
        if(cmd==CAN_BOOT_CMD_END) {
            ++ends;
            const auto valid=next==flash.size() && MetadataValid(reinterpret_cast<const unsigned long*>(flash.data()+256)) &&
                F13_Crc32Bytes(flash.data(),static_cast<unsigned long>(flash.size()-4))==Read(flash.data()+flash.size()-4);
            Check(valid,"END full metadata/image CRC");return Ack(cmd,valid?0U:5U,
                static_cast<uint16_t>(next/4U),Read(flash.data()+flash.size()-4));
        }
        Check(false,"unexpected control");return {};
    }
    std::vector<uint8_t> Burst(const std::vector<uint8_t>& p,host_link::Status& host_status) {
        Check(p.size()==101U && p[0]==CB_FD_VERSION && p[1]==3 && p[9]==CB_FD_FLAGS,
              "BEGIN8 + DATA64 + COMMIT8 in101-byte Host Link payload");
        cb_cursor_t cursor{};
        Check(cb_decode(&cursor,p.data(),static_cast<uint16_t>(p.size()))!=0,"real host decoder");
        struct Record {uint16_t id;uint8_t dlc;std::array<uint8_t,64> data;};
        std::array<Record,3> records{};
        for(auto& r:records)Check(cb_next(&cursor,&r.id,&r.dlc,r.data.data())!=0,"all records");
        Check(records[0].id==0x700 && records[1].id==0x720 && records[1].dlc==15 &&
              records[2].id==0x700 && !cursor.remaining,"FD ID/DLC packing");
        const auto& b=records[0].data;auto data=records[1].data;const auto& c=records[2].data;
        const auto offset=static_cast<unsigned>(b[1]|(b[2]<<8U)|(b[3]<<16U));
        const auto crc=Read(b.data()+4);
        Check(started && g_metadata_sealed && b[0]==0x0B && c[0]==0x0C &&
              Read(c.data()+4)==crc,"transaction boundaries/CRC");
        BlockBegin(offset,crc,next,static_cast<unsigned long>(flash.size()));
        const auto inject=!injected;
        if(inject && fault==Fault::Corrupt)data[0]^=1U;
        if(fault==Fault::BadPadding && g_block_length<64U)data[g_block_length]=0U;
        if(!(inject && fault==Fault::Missing))BlockSegment(0,64,data.data());
        if(inject && fault==Fault::Conflict) {data[0]^=1U;BlockSegment(0,64,data.data());}
        auto status=BlockCheck(offset,crc);
        if(!status && offset!=next) {
            Check(offset+g_block_length==next,"last committed replay only");
            if(std::memcmp(flash.data()+offset,g_block_words,g_block_length))status=CAN_BOOT_STATUS_BAD_SEQUENCE;
        } else if(!status) {
            if(offset>=256 && offset<512 &&
               std::memcmp(g_block_words,reinterpret_cast<uint8_t*>(g_metadata_words)+offset-256,g_block_length))
                status=CAN_BOOT_STATUS_BAD_IMAGE;
            if(!status) {
                std::memcpy(flash.data()+offset,g_block_words,g_block_length);
                writes+=g_block_length/4U;next+=g_block_length;++blocks;
            }
        }
        if(inject && fault==Fault::LostAck && !status)host_status=host_link::Status::Timeout;
        injected=true;
        return Ack(0x0C,status,static_cast<uint16_t>(offset/64U),0x2000+next);
    }
    bool WriteAll(const uint8_t* bytes,std::size_t size,uint32_t,std::wstring*) override {
        host_link::Frame request,response;
        if(!opened || host_link::DecodeFrame(bytes,size,&request)!=host_link::DecodeResult::Ok)return false;
        response.type=host_link::FrameType::Response;response.command=request.command;response.sequence=request.sequence;
        std::vector<uint8_t> can_reply;
        if(request.command==CB_COMMAND) {
            if(fault==Fault::Disconnect) {opened=false;return false;}
            can_reply=Burst(request.payload,response.status);
        } else {
            Check(request.command==0x21 && request.payload.size()>=29 &&
                  request.payload[12]==0x0C && request.payload[13]==0x0F &&
                  request.payload[17]==0x0C,"control FD+BRS / strict match");
            can_reply=Control(request.payload.data()+28);
        }
        if(response.status==host_link::Status::Ok) {
            const auto count=static_cast<uint8_t>(can_reply.size());
            const auto flags=static_cast<uint8_t>(fault==Fault::WrongMode?4U:fault==Fault::ClassicalReply?0U:12U);
            response.payload={0,flags,static_cast<uint8_t>(fault==Fault::WrongDlc?7U:count),count,
                static_cast<uint8_t>(fault==Fault::WrongId?2U:1U),7,0,0,0,0,0,0};
            response.payload.insert(response.payload.end(),can_reply.begin(),can_reply.end());
        }
        std::vector<uint8_t> encoded;host_link::EncodeFrame(response,&encoded);
        rx.insert(rx.end(),encoded.begin(),encoded.end());return true;
    }
    bool ReadSome(std::vector<uint8_t>* bytes,std::size_t max,uint32_t,std::wstring*) override {
        bytes->clear();while(!rx.empty() && bytes->size()<max) {bytes->push_back(rx.front());rx.pop_front();}
        return opened;
    }
};
void Run(const std::vector<uint8_t>& image,Fault fault,bool expected) {
    Link transport(static_cast<unsigned>(image.size()));transport.fault=fault;
    host_link::Client link;can_host::Client can;f13_boot::Client device;std::wstring error;
    link.SetTransport(&transport);can.SetHostLinkClient(&link);device.SetCanClient(&can);device.SetCanFd(true);
    f13_boot::TargetInfo info;
    const auto discovered=device.GetTargetInfo(&info,&error);
    if(fault==Fault::WrongMode || fault==Fault::ClassicalReply || fault==Fault::WrongId || fault==Fault::WrongDlc) {
        Check(!discovered,"wrong mode/BRS/ID/DLC rejected");return;
    }
    Check(discovered,"FD live INFO");
    Check(device.EnterBootloader(&error),"FD one-byte ENTER / two-byte ACK");
    if(discovered)Check(device.SetTargetLayout(info.application_start,info.application_capacity,info.erase_block_size,&error),"live layout");
    f13_boot::UpdateReport report;f13_boot::UpdateCallbacks callbacks;
    const bool ok=discovered && f13_boot::RunUpdate(&device,image,callbacks,&report,&error);
    if(ok!=expected)std::wcerr<<L"FD outcome: "<<error<<L'\n';
    Check(ok==expected,"update outcome");
    if(ok) {
        uint32_t health=0;
        Check(device.GetAppHealth(&health,&error) && health==0x30701U,"FD App health response");
        Check(transport.flash==image && transport.writes==image.size()/4 &&
                transport.ends==1 && report.blocks_acked==(image.size()+63)/64,
                "exact image / one write per word / replay-safe / END");
    }
}
}
int main(int argc,char** argv) {
    static_assert(sizeof(unsigned long)==4,"device word width");
    if(argc!=2)return 2;
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> actual(std::istreambuf_iterator<char>(file),{});
    Check(!actual.empty(),"actual packaged FD App");if(actual.empty())return 1;
    for(auto fault:{Fault::None,Fault::LostAck,Fault::Corrupt,Fault::Missing,Fault::Conflict})Run(actual,fault,true);
    for(auto fault:{Fault::BadPadding,Fault::WrongMode,Fault::ClassicalReply,Fault::WrongId,Fault::WrongDlc,Fault::Disconnect})Run(actual,fault,false);
    std::vector<uint8_t> p{2,1,244,1,1,7,255,7,8,12,0,0,0x20,7,15};
    p.resize(79,0xA5);cb_cursor_t cursor{};
    Check(cb_decode(&cursor,p.data(),static_cast<uint16_t>(p.size()))!=0,"valid FD64 codec");
    for(unsigned i=0;i<p.size();++i)Check(!cb_decode(&cursor,p.data(),static_cast<uint16_t>(i)),"truncation rejected before TX");
    for(unsigned at:{0U,1U,5U,7U,8U,9U,11U,13U,14U}) {
        auto bad=p;bad[at]=0xFF;Check(!cb_decode(&cursor,bad.data(),static_cast<uint16_t>(bad.size())),"malformed header/record");
    }
    p[0]=1;p[9]=0;Check(!cb_decode(&cursor,p.data(),static_cast<uint16_t>(p.size())),"Classical rejects DLC15");
    Check(can_host::BurstFeatureFor(can_host::Mode::CanFd)==CB_FD_FEATURE &&
          can_host::BurstFeatureFor(can_host::Mode::Classical)==CB_FEATURE,
          "UI/preflight/worker share mode-specific capability contract");
    std::cout<<"FD production PC / host codec / device RAM retry/replay/CRC/mode/padding/disconnect models: "
             <<(failures?"FAIL":"PASS")<<'\n';
    return static_cast<int>(failures);
}
