#include "../firmware/common/bridge_boot_link.h"
#include "bridge_update_profile.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <fstream>
#include <iterator>

namespace {
using namespace firmware_upgrade;
struct Memory {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x40000,0xFF);
    unsigned erases=0,writes=0;
    bool escaped=false,fail_write=false;
    static uint8_t Read(void* c,uint32_t a) { return static_cast<Memory*>(c)->bytes.at(a); }
    static uint8_t Erase(void* c,uint32_t a) {
        auto& m=*static_cast<Memory*>(c);
        if (a<0x5000 || a>0x3FC00-1024 || a%1024) { m.escaped=true; return 0; }
        ++m.erases; std::fill_n(m.bytes.begin()+a,1024,static_cast<uint8_t>(0xFF)); return 1;
    }
    static uint8_t Write(void* c,uint32_t a,const uint8_t* data) {
        auto& m=*static_cast<Memory*>(c);
        if (a<0x5000 || a>0x3FC00-4 || a%4) { m.escaped=true; return 0; }
        if (m.fail_write) return 0;
        for(unsigned i=0;i<4;++i) if ((m.bytes[a+i]&data[i])!=data[i]) return 0;
        ++m.writes; std::copy_n(data,4,m.bytes.data()+a); return 1;
    }
    bu_flash_t Backend() { return {this,Read,Erase,Write}; }
};
bu_info_t Info() {
    bu_info_t d={}; d.mode=BU_MODE_BOOT; d.max_data=64; d.flags=BU_FLAG_FLASH_READY;
    d.board_id=0x0101; d.boot_version=0x10300; d.boot_build=20261017;
    d.app_start=0x5000; d.boot_size=0x5000; d.capacity=0x3FC00-0x5000;
    d.flash_size=0x40000; d.erase_size=1024; d.minimum_size=516; d.uart_baud=115200; d.irq_abi=IP_F24_IRQ_ABI;
    std::memcpy(d.part,"R7F124FLJ3xFB",14); std::memcpy(d.family,"RL78/F24",9); return d;
}
std::vector<std::uint8_t> Image(unsigned size) {
    std::vector<std::uint8_t> b(size,0xA6);
    auto* p=b.data()+BU_MANIFEST_OFFSET; std::fill_n(p,BU_METADATA_REGION_SIZE,static_cast<uint8_t>(0xFF)); std::memset(p,0,BU_MANIFEST_SIZE);
    Bu_Write32(p,BU_IMAGE_MAGIC); p[4]=BU_PROTOCOL; p[5]=BU_MANIFEST_SIZE; p[6]=2; p[7]=1;
    Bu_Write16(p+8,0x0101); Bu_Write16(p+10,2); Bu_Write32(p+12,size);
    Bu_Write32(p+16,0x10100); Bu_Write32(p+20,20261015);
    Bu_Write32(p+24,0x5000); Bu_Write32(p+28,0x3FC00-0x5000); Bu_Write32(p+32,0x40000);
    Bu_Write32(p+36,0x5000); Bu_Write32(p+40,1024); Bu_Write32(p+44,516); Bu_Write32(p+48,115200);
    std::memcpy(p+52,"RL78/F24",9); std::memcpy(p+64,"R7F124FLJ3xFB",14); Bu_Write32(p+80,0x10100);
    Bu_Write32(p+96,IP_MAGIC); p[100]=IP_VERSION; p[101]=IP_SIZE; p[102]=IP_ROLE_BRIDGE_APP; p[103]=IP_UART;
    Bu_Write32(p+104,IP_F24_IRQ_ABI); Bu_Write32(p+108,0x10100);
    Bu_Write32(b.data()+size-4,firmware_update::Crc32(b.data(),size-4)); return b;
}
uint8_t Begin(bu_engine_t* engine,const uint8_t* region) {
    if (!(engine->info.flags&BU_FLAG_ACTIVE)) {
        Bu_Abort(engine);
        for (uint16_t at=0;at<BU_METADATA_REGION_SIZE;at+=64) {
            uint8_t page[68]; Bu_Write16(page,at); Bu_Write16(page+2,BU_METADATA_REGION_SIZE);
            std::copy_n(region+at,64,page+4);
            const auto status=Bu_StageMetadata(engine,page,68); if (status) return status;
        }
    }
    uint8_t request[6]; Bu_Write16(request,BU_METADATA_REGION_SIZE);
    Bu_Write32(request+2,firmware_update::Crc32(region,BU_METADATA_REGION_SIZE));
    return Bu_Begin(engine,request,6);
}
class BootTransport final: public host_link::Transport {
public:
    Memory memory;
    bu_engine_t engine={}; bu_link_t wire={}; std::deque<uint8_t> incoming;
    uint8_t drop_once=0xFFU;
    bool corrupt_read_ack=false;
    BootTransport() { const auto d=Info(); const auto flash=memory.Backend();
        Bu_EngineInit(&engine,&d,&flash); Bu_LinkInit(&wire,&engine); }
    bool IsOpen() const override { return true; }
    bool WriteAll(const uint8_t* p,std::size_t n,uint32_t,std::wstring*) override {
        for (std::size_t i=0;i<n;++i) Bu_LinkFeed(&wire,p[i]);
        if (corrupt_read_ack && n>=14U && p[6]==BU_READ_METADATA_COMMAND && wire.reply_length>=18U) {
            wire.reply[12]^=1U;
            Bu_Write16(wire.reply+wire.reply_length-2U,
                host_link::Crc16CcittFalse(wire.reply+12U,wire.reply_length-14U));
        }
        if (n>=14U && p[6]==drop_once) drop_once=0xFFU;
        else incoming.insert(incoming.end(),wire.reply,wire.reply+wire.reply_length);
        wire.reply_length=0; return true;
    }
    bool ReadSome(std::vector<uint8_t>* out,std::size_t maximum,uint32_t,std::wstring*) override {
        out->clear(); while(!incoming.empty() && out->size()<maximum) { out->push_back(incoming.front()); incoming.pop_front(); }
        return true;
    }
};
}
int TestBridgeEngine() {
    int failures=0;
    const auto check=[&](bool ok,const char* name){ if(!ok){++failures; std::cerr<<"FAIL: "<<name<<'\n';}};
    auto bytes=Image(3072); auto info=Info(); Memory memory; auto flash=memory.Backend(); bu_engine_t engine={};
    check(Bu_EngineInit(&engine,&info,&flash) && !Bu_ValidateInstalled(&engine),"erased app stays in recovery");
    check(engine.hold_reason==BU_HOLD_NO_APP,"erased App stay reason");
    auto manifest=std::vector<uint8_t>(bytes.begin()+256,bytes.begin()+512);
    uint8_t begin_request[6]; Bu_Write16(begin_request,256);
    Bu_Write32(begin_request+2,firmware_update::Crc32(manifest.data(),256));
    check(Bu_Begin(&engine,begin_request,6)==BU_SEQUENCE_ERROR && !memory.erases,"incomplete staging cannot erase");
    check(Bu_Begin(&engine,manifest.data(),96)==BU_BAD_ARGUMENT && !memory.erases,"old single BEGIN rejected before erase");
    uint8_t page[68]; Bu_Write16(page,64); Bu_Write16(page+2,256); std::copy_n(manifest.data()+64,64,page+4);
    check(Bu_StageMetadata(&engine,page,68)==BU_SEQUENCE_ERROR && !memory.erases,"future metadata page rejected");
    Bu_Write16(page,0); std::copy_n(manifest.data(),64,page+4);
    check(Bu_StageMetadata(&engine,page,68)==BU_OK && engine.staged==64 && !memory.erases,"metadata page staging has no flash side effect");
    check(Bu_StageMetadata(&engine,page,68)==BU_OK && engine.staged==64,"duplicate metadata page idempotent");
    page[4]^=1; check(Bu_StageMetadata(&engine,page,68)==BU_SEQUENCE_ERROR,"conflicting metadata page rejected"); page[4]^=1;
    Bu_Write16(page+2,255); check(Bu_StageMetadata(&engine,page,68)==BU_BAD_ARGUMENT,"wrong metadata total rejected");
    Bu_Write16(page+2,256); Bu_Write16(page,250); check(Bu_StageMetadata(&engine,page,68)==BU_BAD_ARGUMENT,"metadata range overflow rejected");
    Bu_Abort(&engine); check(!engine.staged && !memory.erases,"ABORT discards staged metadata without erase");
    for (uint16_t at=0;at<256;at+=64) {
        Bu_Write16(page,at); std::copy_n(manifest.data()+at,64,page+4); Bu_StageMetadata(&engine,page,68);
    }
    begin_request[2]^=1;
    check(Bu_Begin(&engine,begin_request,6)==BU_CRC_ERROR && !memory.erases,"wrong whole-region CRC rejected before erase");
    Bu_Abort(&engine);
    auto bad_reserved=manifest; bad_reserved[128]=0;
    check(Begin(&engine,bad_reserved.data())==BU_INCOMPATIBLE && !memory.erases,"unknown extension rejected before erase");
    manifest[8]^=1;
    check(Begin(&engine,manifest.data())==BU_INCOMPATIBLE && !memory.erases,"wrong board before first erase"); manifest[8]^=1;
    check(Begin(&engine,manifest.data())==BU_OK,"BEGIN accepts matching host app");
    check(Begin(&engine,manifest.data())==BU_OK && memory.erases==1,"duplicate BEGIN no second erase");
    std::vector<uint8_t> chunk(68);
    for(uint32_t offset=0;offset<bytes.size();offset+=64) {
        Bu_Write32(chunk.data(),offset); std::copy_n(bytes.data()+offset,64,chunk.data()+4);
        check(Bu_Data(&engine,chunk.data(),68)==BU_OK,"sequential write");
        const auto writes=memory.writes;
        check(Bu_Data(&engine,chunk.data(),68)==BU_OK && memory.writes==writes,"duplicate DATA readback no second write");
        chunk[4]^=1;
        check(Bu_Data(&engine,chunk.data(),68)==BU_SEQUENCE_ERROR && memory.writes==writes,"conflicting DATA retry rejected"); chunk[4]^=1;
    }
    check(memory.erases==3 && !memory.escaped,"erase only image-covered app blocks");
    check(Bu_Read32(memory.bytes.data()+0x5100)==0xFFFFFFFF,"commit magic withheld until END");
    bu_engine_t reboot={}; Bu_EngineInit(&reboot,&info,&flash);
    check(!Bu_ValidateInstalled(&reboot),"power loss before END cannot jump");
    check(Bu_End(&engine,bytes.data()+bytes.size()-4,4)==BU_OK && Bu_ValidateInstalled(&engine),"END CRC and commit / actual readback");
    check(std::equal(bytes.begin(),bytes.end(),memory.bytes.begin()+0x5000),"every flashed byte equals PC image");
    memory.bytes[0x5600]^=1; check(!Bu_ValidateInstalled(&engine),"startup detects post-commit corruption");
    check(engine.hold_reason==BU_HOLD_CRC,"installed CRC stay reason");
    check(Begin(&engine,manifest.data())==BU_OK,"new update restarts at zero");
    Bu_Write32(chunk.data(),4); check(Bu_Data(&engine,chunk.data(),68)==BU_SEQUENCE_ERROR,"future offset rejected");
    Bu_Write32(chunk.data(),0); std::copy_n(bytes.data(),64,chunk.data()+4); memory.fail_write=true;
    check(Bu_Data(&engine,chunk.data(),68)==BU_FLASH_ERROR && !(engine.info.flags&BU_FLAG_FLASH_READY),"flash failure disables writes until recovery");
    check(Begin(&engine,manifest.data())==BU_BAD_STATE,"fault does not silently retry erase");
    Memory corrupt; auto corrupt_flash=corrupt.Backend(); bu_engine_t bad={}; Bu_EngineInit(&bad,&info,&corrupt_flash);
    Begin(&bad,manifest.data());
    for(uint32_t offset=0;offset<bytes.size();offset+=64) {
        Bu_Write32(chunk.data(),offset); std::copy_n(bytes.data()+offset,64,chunk.data()+4);
        if (!offset) chunk[4]^=1;
        check(Bu_Data(&bad,chunk.data(),68)==BU_OK,"staged CRC-negative data reaches END");
    }
    check(Bu_End(&bad,bytes.data()+bytes.size()-4,4)==BU_CRC_ERROR &&
        Bu_Read32(corrupt.bytes.data()+0x5100)==0xFFFFFFFF && !Bu_ValidateInstalled(&bad),"bad final CRC cannot commit or jump");
    BootTransport bridge; host_link::Client client; client.SetTransport(&bridge); client.BeginSession();
    host_link::HostInfo host; host_link::Capabilities caps; std::wstring error;
    check(client.Probe(0,&host,&caps,&error),"real boot parser interoperates with PC generic probe, boot-only caps");
    check(caps.maximum_host_link_payload==128 && client.Ping(std::vector<uint8_t>(128,0x5A),&error),"real C parser/client128-byte boundary echo");
    bu_info_t live={}; check(bridge_update::ReadInfo(client,&live,&error),"real bridge descriptor interoperates");
    bool aborted=false;
    check(bridge_update::Program(client,live,bytes,{}, {},[&](){aborted=true;},&error),"PC uploader -> real C parser / engine -> mock flash");
    check(!aborted && !bridge.wire.action && bridge.engine.info.flags&BU_FLAG_VALID_APP,"END does not auto RUN");
    check(!bridge.memory.escaped && bridge.memory.erases==3,"end-to-end boot region untouched");
    std::vector<uint8_t> actual_region;
    check(bridge_update::ReadMetadata(client,&actual_region,&error) && actual_region==manifest,"four read pages reassemble exact256-byte metadata");
    bridge.corrupt_read_ack=true;
    check(!bridge_update::ReadMetadata(client,&actual_region,&error) && actual_region.empty(),"wrong metadata page ACK exposes no partial result");
    bridge.corrupt_read_ack=false;
    const auto before_stage=bridge.memory.erases;
    Bu_Write16(page,0); Bu_Write16(page+2,256); std::copy_n(manifest.data(),64,page+4);
    Bu_Abort(&bridge.engine);
    check(Bu_StageMetadata(&bridge.engine,page,68)==BU_OK && bridge.memory.erases==before_stage,"new staged metadata leaves installed app unchanged");
    bu_engine_t stage_reboot={}; auto old_flash=bridge.memory.Backend();
    Bu_EngineInit(&stage_reboot,&info,&old_flash);
    check(!stage_reboot.staged && Bu_ValidateInstalled(&stage_reboot),"power loss before BEGIN preserves old validated app");
    BootTransport stage_cancel; host_link::Client cancel_stage_client; cancel_stage_client.SetTransport(&stage_cancel);
    bool stage_cleaned=false;
    const auto stage_cancelled=[&](){return stage_cancel.engine.staged>=64U;};
    cancel_stage_client.SetCancelCheck(stage_cancelled);
    const auto clean_stage=[&](){
        stage_cleaned=true; cancel_stage_client.SetCancelCheck([](){return false;});
        host_link::Response r; std::wstring ignored;
        cancel_stage_client.Request(static_cast<host_link::Command>(BU_ABORT_COMMAND),{},&r,&ignored);
    };
    check(!bridge_update::Program(cancel_stage_client,info,bytes,stage_cancelled,{},clean_stage,&error) && stage_cleaned &&
        !stage_cancel.memory.erases && !stage_cancel.memory.writes && !stage_cancel.engine.staged,
        "cancel during metadata staging cleans up with zero flash side effects");
    BootTransport stage_retry; host_link::Client stage_client; stage_client.SetTransport(&stage_retry);
    bu_info_t stage_info={}; bridge_update::ReadInfo(stage_client,&stage_info,&error); stage_retry.drop_once=BU_STAGE_METADATA_COMMAND;
    check(bridge_update::Program(stage_client,stage_info,bytes,{}, {},[]{},&error) && stage_retry.memory.erases==3 &&
        stage_client.Stats().retries==1,"lost metadata ACK replays once without erase/write duplication");
    BootTransport retry; host_link::Client retry_client; retry_client.SetTransport(&retry); retry_client.BeginSession();
    bu_info_t retry_info={}; bridge_update::ReadInfo(retry_client,&retry_info,&error); retry.drop_once=BU_BEGIN_COMMAND;
    check(bridge_update::Program(retry_client,retry_info,bytes,{}, {},[]{},&error) && retry.memory.erases==3 &&
        retry_client.Stats().retries==1,"lost BEGIN ACK replays without a second erase");
    BootTransport cancel; host_link::Client cancel_client; cancel_client.SetTransport(&cancel); cancel_client.BeginSession();
    bu_info_t cancel_info={}; bridge_update::ReadInfo(cancel_client,&cancel_info,&error);
    bool cancellation=false,cleanup=false; cancel_client.SetCancelCheck([&](){return cancellation;});
    const auto abort=[&](){
        cleanup=true; cancel_client.SetCancelCheck([](){return false;});
        host_link::Response response; std::wstring ignored;
        cancel_client.Request(static_cast<host_link::Command>(BU_ABORT_COMMAND),{},&response,&ignored);
    };
    check(!bridge_update::Program(cancel_client,cancel_info,bytes,[&](){return cancellation;},
        [&](int){cancellation=true;},abort,&error) && cleanup && !(cancel.engine.info.flags&BU_FLAG_ACTIVE) &&
        !Bu_ValidateInstalled(&cancel.engine),"cancellation sends fresh ABORT with cancel suppressed and retains boot");
    for (unsigned size:{60U*1024U,110U*1024U}) {
        BootTransport larger; host_link::Client upload; upload.SetTransport(&larger); upload.BeginSession();
        bu_info_t descriptor={}; auto grown=Image(size); error.clear();
        check(bridge_update::ReadInfo(upload,&descriptor,&error) &&
            bridge_update::Program(upload,descriptor,grown,{}, {},[]{},&error),"60K /110K dynamic app length and trailing CRC");
        check(!larger.memory.escaped && std::equal(grown.begin(),grown.end(),larger.memory.bytes.begin()+0x5000),"grown image exact contents and no boot overwrite");
    }
    std::cout<<"Bridge real C engine/parser and PC uploader tests: "<<(failures ? "FAIL" : "PASS")<<'\n'; return failures;
}
int TestPackagedBridge(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if (!file) { std::cerr<<"FAIL: packaged bridge image not found\n"; return 1; }
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    const auto inspection=bridge_update::Inspect(bytes);
    BootTransport bridge; host_link::Client client; client.SetTransport(&bridge); client.BeginSession();
    bu_info_t descriptor={}; std::wstring error;
    const bool ok=inspection.valid && bridge_update::ReadInfo(client,&descriptor,&error) &&
        bridge_update::Program(client,descriptor,bytes,{}, {},[]{},&error) &&
        !bridge.memory.escaped && std::equal(bytes.begin(),bytes.end(),bridge.memory.bytes.begin()+0x5000);
    std::cout<<"Actual packaged bridge BIN -> real parser / engine: "<<(ok ? "PASS" : "FAIL")<<" ("<<bytes.size()<<" bytes)\n";
    return ok ? 0 : 1;
}
int TestPackagedCan(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if (!file) { std::cerr<<"FAIL: packaged CAN image not found\n"; return 1; }
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    const auto image=firmware_update::InspectImage(bytes);
    const bool ok=image.metadata_valid && image.checksum_matches && image.metadata.format==6U &&
        image.metadata.manifest_length==128U && image.metadata.image_size==bytes.size();
    std::cout<<"Actual packaged F13 format6 / policy / metadata256 / CRC: "<<(ok?"PASS":"FAIL")<<" ("<<bytes.size()<<" bytes)\n";
    return ok?0:1;
}
