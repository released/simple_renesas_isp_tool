#include "f13_boot_profile.h"
#include "f13_boot_update.h"
#include "firmware_update_profile.h"
#include "device_version_policy.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include "../firmware/common/can_burst.h"
#include "../firmware/common/can_burst.inc"

/* Execute the production C90 RAM staging/CRC. CAN/FSL and power are modeled,
 * not measured hardware. The PC client/parser/update runner are unmodified. */
namespace block_target {
#define __far
#define F13_CRC_HOST_TEST
#include "can_boot_transport.h"
#include "sw_crc32.c"
#include "can_boot_metadata.inc"
#include "can_boot_verify.inc"
#include "can_boot_block.inc"
#undef __far
}

namespace {
using namespace firmware_upgrade;
unsigned failures;
uint32_t IP_Read32(const uint8_t* p) { return image_policy::Read32(p); }
void IP_Write32(uint8_t* p, uint32_t value) {
    for (unsigned i=0;i<4U;++i) p[i]=static_cast<uint8_t>(value>>(8U*i));
}
void Check(bool ok, const char* text) {
    if (!ok) { ++failures; std::cerr << "FAIL block: " << text << '\n'; }
}
void Put32(std::vector<uint8_t>& p, uint32_t value) {
    for (unsigned i=0; i<4; ++i) p.push_back(static_cast<uint8_t>(value>>(i*8U)));
}
std::vector<uint8_t> Image(unsigned size) {
    std::vector<uint8_t> image(size,0xA6U);
    std::memcpy(image.data()+256,block_target::g_image_metadata_contract,128);
    std::fill_n(image.data()+384,128,static_cast<uint8_t>(0xFFU));
    IP_Write32(image.data()+272,0x10500U);
    IP_Write32(image.data()+276,20261015U);
    IP_Write32(image.data()+280,size);
    IP_Write32(image.data()+364,0x10500U);
    IP_Write32(image.data()+size-4,firmware_update::Crc32(image.data(),size-4));
    return image;
}
enum class Fault { None, Missing, Crc, Conflict, LostAck, WrongAck, Flash, Disconnect, AlwaysMissing };

class TargetLink final : public host_link::Transport {
    std::deque<uint8_t> rx;
    std::vector<uint8_t> cached;
    uint16_t last_sequence=0;
    bool injected=false;
    bool started=false;
    uint32_t size=0;
public:
    Fault fault=Fault::None;
    std::vector<uint8_t> flash;
    unsigned next=0, writes=0, blocks=0, ends=0, aborts=0, bursts=0, packets=0;
    bool reverse=false, duplicates=false, power_checks=false, opened=true;
    bool app_mode=false;
    explicit TargetLink(unsigned image_size) : flash(image_size,0xFFU) {
        block_target::MetadataReset(); block_target::g_block_bad=1U;
    }
    bool IsOpen() const override { return opened; }
    bool BootValid() const {
        return flash[0]!=0xFFU && block_target::MetadataValid(
            reinterpret_cast<const unsigned long*>(flash.data()+256)) &&
            block_target::F13_Crc32Bytes(flash.data(),static_cast<unsigned long>(flash.size()-4))==IP_Read32(flash.data()+flash.size()-4);
    }
    std::vector<uint8_t> Ack(uint8_t cmd,uint8_t status,uint16_t detail,uint32_t value) {
        std::vector<uint8_t> p{cmd,status,static_cast<uint8_t>(detail),static_cast<uint8_t>(detail>>8U)};
        Put32(p,value); return p;
    }
    std::vector<uint8_t> Control(const std::vector<uint8_t>& p) {
        using namespace block_target;
        const auto cmd=p[0];
        if (cmd==CAN_BOOT_CMD_INFO) {
            const auto page=p[1]; uint32_t value=0;
            if(page==0x1BU) return Ack(cmd,0,static_cast<uint16_t>(page|(app_mode?0x0100U:0x0200U)),app_mode?0x30701U:0U);
            if(page==0x1CU) return Ack(cmd,0,static_cast<uint16_t>(page|(app_mode?0x0100U:0x0200U)),app_mode?1U:7U);
            const uint32_t info[]={0x0605F13BU,IMAGE_BOOT_VERSION,IMAGE_BOOT_BUILD,0xFFFFFFFFU,0xFFFFFFFFU,
                0x2000U,0x1E000U,0x20000U,0x2000U,1024U,0x01010001U,4U,0U,0x700U,0x701U,
                IMAGE_MCU_PART_0,IMAGE_MCU_PART_1,0U,0U,IMAGE_MCU_FAMILY_0,
                IMAGE_MCU_FAMILY_1,0U,0xFFFFFFFFU,516U,IMAGE_POLICY_WORD,IMAGE_IRQ_ABI,CAN_BOOT_BLOCK_INFO};
            if(page==CAN_BOOT_INFO_PAGE_VERSION_POLICY) value=BootValid() ? 1U|(IP_Read32(flash.data()+0x174U)<<1U) : 0U;
            else { Check(page < 27U,"INFO page bounds"); if (page < 27U) value=info[page]; }
            if(BootValid() && (page==3U || page==4U || page==22U))
                value=IP_Read32(flash.data()+256U+(page==3U?16U:page==4U?20U:24U));
            return Ack(cmd,0,static_cast<uint16_t>(page | (app_mode?0x0100U:0x0200U)),value);
        }
        if (cmd==CAN_BOOT_CMD_METADATA_RESET) { MetadataReset(); return Ack(cmd,0,0,256); }
        if (cmd==CAN_BOOT_CMD_METADATA) {
            const auto status=MetadataStage(static_cast<uint16_t>(p[1] | (p[2]<<8U)),p.data()+4);
            return Ack(cmd,status,g_metadata_staged,256);
        }
        if (cmd==CAN_BOOT_CMD_SEAL) return Ack(cmd,MetadataSeal(IP_Read32(p.data()+4)),g_metadata_staged,256);
        if (cmd==CAN_BOOT_CMD_START || cmd==CAN_BOOT_CMD_FORCE_START) {
            const auto requested=IP_Read32(p.data()+4);
            if (!g_metadata_sealed || requested!=g_metadata_words[6]) return Ack(cmd,CAN_BOOT_STATUS_BAD_IMAGE,0,0);
            if(started) return Ack(cmd,0,0,0x2000);
            const auto policy=BootValid()?1U|(IP_Read32(flash.data()+0x174U)<<1U):0U;
            const auto installed=IP_Read32(flash.data()+0x110U);
            if(!VersionAllowed(policy,installed,cmd)) return Ack(cmd,CAN_BOOT_STATUS_VERSION,0,installed);
            size=requested; started=true; next=0; return Ack(cmd,0,0,0x2000);
        }
        if (cmd==CAN_BOOT_CMD_END) { ++ends; app_mode=BootValid(); started=false;
            return Ack(cmd,app_mode?0U:CAN_BOOT_STATUS_CRC_ERROR,
            static_cast<uint16_t>(next/4U),block_target::F13_Crc32Bytes(flash.data(),size-4)); }
        if (cmd==CAN_BOOT_CMD_VERIFY) {
            if(started || app_mode) return Ack(cmd,CAN_BOOT_STATUS_BAD_STATE,0,0);
            unsigned long count=0,crc=0;
            const auto status=VerifyImage(reinterpret_cast<const unsigned long*>(flash.data()),&count,&crc);
            return Ack(cmd,status,0,crc);
        }
        if (cmd==CAN_BOOT_CMD_ABORT) { ++aborts; started=false; return Ack(cmd,0,static_cast<uint16_t>(next/4U),0x2000U+next); }
        return Ack(cmd,CAN_BOOT_STATUS_BAD_COMMAND,0,0);
    }
    std::vector<uint8_t> Burst(const std::vector<uint8_t>& p, host_link::Status& status) {
        using namespace block_target;
        cb_cursor_t cursor{};
        Check(cb_decode(&cursor,p.data(),static_cast<uint16_t>(p.size()))!=0,"production F24 burst decoder accepts PC payload");
        struct Segment { uint16_t id; uint8_t dlc; uint8_t data[8]; };
        std::vector<Segment> frames;
        while (cursor.remaining) { Segment frame{}; cb_next(&cursor,&frame.id,&frame.dlc,frame.data); frames.push_back(frame); }
        ++bursts; packets+=static_cast<unsigned>(frames.size());
        Check(started && g_metadata_sealed,"no data before metadata seal/START");
        const auto& begin=frames.front(); const auto& commit=frames.back();
        Check(begin.data[0]==0x0BU && commit.data[0]==0x0CU,"BEGIN / COMMIT framing");
        const auto offset=static_cast<uint32_t>(begin.data[1] | (begin.data[2]<<8U) | (begin.data[3]<<16U));
        const auto crc=IP_Read32(begin.data+4);
        BlockBegin(offset,crc,next,size);
        if (reverse) std::reverse(frames.begin()+1,frames.end()-1);
        const bool inject=!injected || fault==Fault::AlwaysMissing;
        for (std::size_t i=1;i+1<frames.size();++i) {
            auto& frame=frames[i]; const auto index=static_cast<uint8_t>(frame.id-0x720U);
            Check(index<8 && frame.dlc<=8,"indexed raw data ID/DLC");
            if (inject && (fault==Fault::Missing || fault==Fault::AlwaysMissing) && index==2U) continue;
            if (inject && fault==Fault::Crc && index==2U) frame.data[0]^=1U;
            BlockSegment(index,frame.dlc,frame.data);
            if (duplicates) BlockSegment(index,frame.dlc,frame.data);
            if (inject && fault==Fault::Conflict && index==2U) {
                frame.data[0]^=1U; BlockSegment(index,frame.dlc,frame.data);
            }
        }
        const auto commit_offset=static_cast<uint32_t>(commit.data[1] | (commit.data[2]<<8U) | (commit.data[3]<<16U));
        auto result=BlockCheck(commit_offset,IP_Read32(commit.data+4));
        if (result==0U && offset!=next) {
            Check(offset+g_block_length==next,"only last committed block may replay");
            if (std::memcmp(flash.data()+offset,g_block_words,g_block_length)) result=CAN_BOOT_STATUS_BAD_SEQUENCE;
        } else if (result==0U) {
            if (offset>=256U && offset<512U && std::memcmp(g_block_words,
                reinterpret_cast<const uint8_t*>(g_metadata_words)+offset-256U,g_block_length)) result=CAN_BOOT_STATUS_BAD_IMAGE;
            if (result==0U) for (unsigned at=0;at<g_block_length;at+=4U) {
                if (inject && fault==Fault::Flash && writes==6U) { result=CAN_BOOT_STATUS_FLASH_ERROR; break; }
                std::memcpy(flash.data()+offset+at,reinterpret_cast<const uint8_t*>(g_block_words)+at,4);
                ++writes;
                if (power_checks) Check(BootValid()==(offset+at+4U==size),"power cut after word cannot launch incomplete app");
            }
            if (result==0U) { next+=g_block_length; ++blocks; }
        }
        if (inject && fault==Fault::LostAck && result==0U) status=host_link::Status::Timeout;
        const unsigned next_error=(inject && fault==Fault::WrongAck)?4U:0U;
        injected=true;
        return Ack(0x0CU,result,static_cast<uint16_t>(offset/64U),0x2000U+next+next_error);
    }
    bool WriteAll(const uint8_t* data,std::size_t length,uint32_t,std::wstring* error) override {
        host_link::Frame request,reply;
        if (!opened || host_link::DecodeFrame(data,length,&request)!=host_link::DecodeResult::Ok) return false;
        if (request.sequence==last_sequence) { rx.insert(rx.end(),cached.begin(),cached.end()); return true; }
        reply.type=host_link::FrameType::Response; reply.sequence=request.sequence; reply.command=request.command;
        std::vector<uint8_t> can_reply;
        if (request.command==CB_COMMAND) {
            if (fault==Fault::Disconnect) { opened=false; if (error) *error=L"Injected disconnect"; return false; }
            can_reply=Burst(request.payload,reply.status);
        } else {
            Check(request.command==0x21U && request.payload.size()>=29U,"normal control uses21");
            can_reply=Control({request.payload.begin()+28,request.payload.end()});
        }
        if (reply.status==host_link::Status::Ok) {
            reply.payload={0,0,8,8,1,7,0,0,0,0,0,0};
            reply.payload.insert(reply.payload.end(),can_reply.begin(),can_reply.end());
        }
        host_link::EncodeFrame(reply,&cached); last_sequence=request.sequence;
        rx.insert(rx.end(),cached.begin(),cached.end()); return true;
    }
    bool ReadSome(std::vector<uint8_t>* bytes,std::size_t maximum,uint32_t,std::wstring*) override {
        bytes->clear();
        while (!rx.empty() && bytes->size()<maximum) { bytes->push_back(rx.front()); rx.pop_front(); }
        return opened;
    }
};

void Run(const std::vector<uint8_t>& image, Fault fault, bool expected_success,
         bool reverse=false, bool duplicate=false, bool cuts=false, bool cancel=false) {
    TargetLink target(static_cast<unsigned>(image.size())); target.fault=fault;
    target.reverse=reverse; target.duplicates=duplicate; target.power_checks=cuts;
    host_link::Client link; can_host::Client can; f13_boot::Client device;
    link.SetTransport(&target); can.SetHostLinkClient(&link); device.SetCanClient(&can);
    f13_boot::TargetInfo info; std::wstring error;
    Check(device.GetTargetInfo(&info,&error),"v5 live INFO capability validated");
    uint32_t flags=0;
    Check(device.GetDiagnostics(&flags,&error) && flags==7U,"live Boot advertises diagnostics; not inferred from MCU/version");
    f13_boot::UpdateReport report; f13_boot::UpdateCallbacks callbacks;
    std::vector<f13_boot::UpdateStage> stages;
    callbacks.stage=[&](f13_boot::UpdateStage stage){stages.push_back(stage);};
    bool cleanup=false;
    if (cancel) {
        callbacks.is_canceled=[&] { return target.blocks>=2U; };
        link.SetCancelCheck([&] { return target.blocks>=2U && !cleanup; });
        callbacks.abort=[&](f13_boot::Ack* ack,std::wstring* reason) {
            cleanup=true;const bool result=device.Abort(ack,reason);cleanup=false;return result;
        };
    }
    const bool success=f13_boot::RunUpdate(&device,image,callbacks,&report,&error);
    Check(success==expected_success,"end-to-end update outcome");
    if (success) {
        uint32_t health=0;
        Check(device.GetAppHealth(&health,&error) && health==0x30701U,"production PC health page and App-state decode");
        target.app_mode=false;
        const auto original=target.flash; const auto writes=target.writes;
        f13_boot::Ack verified;
        Check(device.VerifyInstalled(&verified,&error) && verified.value==IP_Read32(image.data()+image.size()-4) &&
            target.flash==original && target.writes==writes,"production PC VERIFY and real read-only device CRC helper");
        Check(stages==std::vector<f13_boot::UpdateStage>{f13_boot::UpdateStage::Metadata,
            f13_boot::UpdateStage::Start,f13_boot::UpdateStage::Transfer,f13_boot::UpdateStage::End},
            "production update stage order and final END");
        Check(target.flash==image && target.BootValid(),"exact image and final boot CRC match");
        Check(target.writes==image.size()/4U,"whole-block retry never rewrites Flash");
        Check(target.blocks==(image.size()+63U)/64U && report.blocks_acked==target.blocks,"block count ceiling");
        Check(target.ends==1U && target.aborts==0U,"only final CRC allows END success");
    } else {
        if(cancel) Check(report.abort_acknowledged && target.opened,"independent cancel suppresses cancellation only for bounded ABORT; transport remains open");
        Check(!target.BootValid() && target.ends==0U && report.abort_attempted,"fault/cancel retains boot and attempts ABORT");
        if (fault==Fault::AlwaysMissing) Check(target.writes==0U && target.bursts==2U,"bounded retry / no erase-write on missing segment");
    }
}
}

int TestCanBlock(const char* actual_image) {
    failures=0;
    static_assert(sizeof(unsigned long)==4,"F13 words are32bit");
    {
        auto installed=Image(640U), older=installed, still_older=installed;
        IP_Write32(installed.data()+0x110U,0x10600U); IP_Write32(installed.data()+0x174U,1U);
        IP_Write32(older.data()+0x110U,0x10500U); IP_Write32(older.data()+0x174U,1U);
        IP_Write32(still_older.data()+0x110U,0x10400U); IP_Write32(still_older.data()+0x174U,0U);
        for(auto* image:{&installed,&older,&still_older})
            IP_Write32(image->data()+image->size()-4,firmware_update::Crc32(image->data(),image->size()-4));
        TargetLink target(640U); target.flash=installed;
        host_link::Client link; can_host::Client can; f13_boot::Client device;
        link.SetTransport(&target);can.SetHostLinkClient(&link);device.SetCanClient(&can);
        f13_boot::TargetInfo live; std::wstring error;
        Check(device.GetTargetInfo(&live,&error) && live.version_policy==3U && live.app_version==0x10600U,
            "live protected installed identity read before force update");
        f13_boot::UpdateReport report;f13_boot::UpdateCallbacks normal,force;
        Check(!f13_boot::RunUpdate(&device,older,normal,&report,&error) && target.writes==0U && target.flash==installed,
            "normal downgrade rejected at START before erase/write");
        Check(error.find(L"VERSION_DOWNGRADE")!=std::wstring::npos,"Boot downgrade status is decoded by production client");
        force.force_old_version=true;
        force.is_canceled=[] {return true;};
        Check(!f13_boot::RunUpdate(&device,older,force,&report,&error) && target.flash==installed,
            "canceled force does not arm permission or write Flash");
        force.is_canceled={};
        Check(f13_boot::RunUpdate(&device,older,force,&report,&error) && target.flash==older,
            "request-local force START completes older protected image with all metadata/CRC checks");
        target.app_mode=false;
        const auto writes=target.writes;
        Check(!f13_boot::RunUpdate(&device,still_older,normal,&report,&error) && target.writes==writes && target.flash==older,
            "next normal update rejects lower disable BIN: force permission never persists");
        auto corrupt=still_older;corrupt.back()^=1U;
        Check(!f13_boot::RunUpdate(&device,corrupt,force,&report,&error) && target.writes==writes,
            "force cannot bypass image CRC check");
        for(const auto offset:{0x10CU,0x11CU,0x168U,0x16CU}) {
            auto incompatible=still_older; incompatible[offset]^=0x80U;
            if(offset==0x16CU) IP_Write32(incompatible.data()+offset,IMAGE_BOOT_VERSION+1U);
            IP_Write32(incompatible.data()+incompatible.size()-4,
                firmware_update::Crc32(incompatible.data(),incompatible.size()-4));
            Check(!f13_boot::RunUpdate(&device,incompatible,force,&report,&error) &&
                target.writes==writes && target.flash==older,
                "force cannot bypass target/layout/IRQ ABI/minimum Boot even with correct CRC");
        }
    }
    for (unsigned tail=0;tail<64;tail+=4) Run(Image(3072U+tail),Fault::None,true,true,true,tail==4U);
    for (const auto fault : {Fault::Missing,Fault::Crc,Fault::Conflict,Fault::LostAck}) Run(Image(3076),fault,true);
    for (const auto fault : {Fault::WrongAck,Fault::Flash,Fault::Disconnect,Fault::AlwaysMissing}) Run(Image(3076),fault,false);
    Run(Image(3076),Fault::None,false,false,false,false,true);
    Run(Image(19224),Fault::None,true);
    if (actual_image) {
        std::ifstream file(actual_image,std::ios::binary);
        std::vector<uint8_t> image(std::istreambuf_iterator<char>(file),{});
        Check(!image.empty(),"actual v5 packaged image exists"); if (!image.empty()) Run(image,Fault::LostAck,true);
    }
    // Reject malformed bursts completely before any frame can reach a backend.
    std::vector<uint8_t> valid{1,1,244,1,1,7,255,7,8,0,0,0,0,7,8,1,2,3,4,5,6,7,8};
    cb_cursor_t cursor{};
    Check(cb_decode(&cursor,valid.data(),static_cast<uint16_t>(valid.size()))!=0,"valid generic burst");
    for (unsigned n=0;n<valid.size();++n) Check(!cb_decode(&cursor,valid.data(),static_cast<uint16_t>(n)),"truncation rejected before TX");
    for (unsigned at : {0U,1U,5U,7U,8U,9U,11U,13U,14U}) {
        auto invalid=valid; invalid[at]=0xFFU;
        Check(!cb_decode(&cursor,invalid.data(),static_cast<uint16_t>(invalid.size())),"bad version/count/ID/mask/DLC/flags/gap rejected");
    }
    valid.push_back(0U); Check(!cb_decode(&cursor,valid.data(),static_cast<uint16_t>(valid.size())),"trailing bytes rejected");
    std::cout << "CAN v5 PC/parser + real F24 burst codec + real F13 block/CRC, fault/replay/tail/power-cut models: "
              << (failures?"FAIL":"PASS") << '\n';
    return static_cast<int>(failures);
}
