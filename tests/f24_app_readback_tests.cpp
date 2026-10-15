#include "bridge_update_profile.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>

extern "C" {
// Compile the real App metadata handler; only flash reads/board identity are mocked.
#include "../firmware/RL78_F24_Bridge_app/bridge_app.c"
}

namespace {
std::vector<uint8_t> installed;
unsigned escaped_reads=0, reset_requests=0;
using namespace firmware_upgrade;
class AppTransport final: public host_link::Transport {
    std::deque<uint8_t> pending;
public:
    unsigned metadata_pages=0;
    bool IsOpen() const override { return true; }
    bool WriteAll(const uint8_t* bytes,std::size_t count,uint32_t,std::wstring*) override {
        host_link::Frame request,reply;
        if (host_link::DecodeFrame(bytes,count,&request)!=host_link::DecodeResult::Ok) return false;
        uint8_t payload[HOST_WIRE_MAX_PAYLOAD]={},status=BU_OK;
        uint16_t length=0;
        const auto owned=F24Bridge_AppCommand(request.command,request.payload.data(),
            static_cast<uint16_t>(request.payload.size()),payload,&length,&status);
        if (!owned) status=2U;
        if (request.command==BU_READ_METADATA_COMMAND) ++metadata_pages;
        reply.type=host_link::FrameType::Response; reply.command=request.command;
        reply.sequence=request.sequence; reply.status=static_cast<host_link::Status>(status);
        reply.payload.assign(payload,payload+length);
        std::vector<uint8_t> wire;
        if (!host_link::EncodeFrame(reply,&wire)) return false;
        pending.insert(pending.end(),wire.begin(),wire.end()); return true;
    }
    bool ReadSome(std::vector<uint8_t>* bytes,std::size_t maximum,uint32_t,std::wstring*) override {
        bytes->clear();
        while (!pending.empty() && bytes->size()<maximum) { bytes->push_back(pending.front()); pending.pop_front(); }
        return true;
    }
};
}

extern "C" uint8_t F24Bridge_Read(void*,uint32_t address) {
    if (address<F24_BRIDGE_APP_START || address-F24_BRIDGE_APP_START>=installed.size()) {
        ++escaped_reads; return 0xFFU;
    }
    return installed[address-F24_BRIDGE_APP_START];
}
extern "C" void F24Bridge_Info(bu_info_t* d,uint8_t mode) {
    std::memset(d,0,sizeof(*d)); d->mode=mode; d->max_data=BU_MAX_DATA;
    d->board_id=F24_BRIDGE_BOARD_ID; d->boot_version=F24_BRIDGE_BOOT_VERSION;
    d->boot_build=F24_BRIDGE_BUILD; d->app_start=F24_BRIDGE_APP_START;
    d->boot_size=F24_BRIDGE_APP_START; d->capacity=F24_BRIDGE_APP_LIMIT-F24_BRIDGE_APP_START;
    d->flash_size=F24_BRIDGE_FLASH_SIZE; d->erase_size=F24_BRIDGE_ERASE_SIZE;
    d->minimum_size=F24_BRIDGE_MINIMUM_SIZE; d->uart_baud=F24_BRIDGE_UART_BAUD;
    d->irq_abi=IP_F24_IRQ_ABI;
    std::memcpy(d->part,"R7F124FLJ3xFB",14U); std::memcpy(d->family,"RL78/F24",9U);
}
extern "C" void F24Bridge_ResetToBoot(void) { ++reset_requests; }

int TestF24AppReadback(const char* path) {
    int failures=0;
    const auto check=[&](bool pass,const char* name) { if (!pass) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; } };
    std::ifstream file(path,std::ios::binary);
    installed.assign(std::istreambuf_iterator<char>(file),{});
    const auto image=firmware_upgrade::bridge_update::Inspect(installed);
    check(image.valid,"App readback fixture is an actual valid packaged image");
    if (!image.valid) return failures;
    g_app_inspection={}; g_info_ready=0; g_enter_pending=0; escaped_reads=reset_requests=0;
    const auto original=installed;
    AppTransport app;
    firmware_upgrade::host_link::Client client; client.SetTransport(&app);
    for (int reopen=0;reopen<2;++reopen) {
        client.BeginSession(); bu_info_t info={}; std::wstring error;
        check(firmware_upgrade::bridge_update::ReadInfo(client,&info,&error),"fresh/reopened PC session validates App descriptor and all256metadata bytes");
        check(info.mode==BU_MODE_APP && (info.flags&BU_FLAG_VALID_APP) && !(info.flags&BU_FLAG_FLASH_READY),"App reports valid installed image but cannot write flash");
        check(info.app_version==image.metadata.version && info.installed_size==installed.size(),"App live readback matches actual installed BIN");
    }
    check(app.metadata_pages==8U,"both sessions read four64-byte pages, not descriptor-only fallback");
    uint8_t request[4]={},reply[HOST_WIRE_MAX_PAYLOAD]={},status=BU_OK;
    uint16_t length=0;
    Bu_Write16(request,256U); Bu_Write16(request+2,64U);
    check(F24Bridge_AppCommand(BU_READ_METADATA_COMMAND,request,4U,reply,&length,&status) &&
        status==BU_BAD_ARGUMENT && !length,"App metadata offset outside region rejected");
    for (const auto command:{BU_BEGIN_COMMAND,BU_DATA_COMMAND,BU_END_COMMAND,BU_ABORT_COMMAND,BU_RUN_COMMAND,BU_STAGE_METADATA_COMMAND})
        check(!F24Bridge_AppCommand(static_cast<uint8_t>(command),request,4U,reply,&length,&status),"boot-only write/run/stage commands remain unowned by App");
    check(installed==original && !escaped_reads && !reset_requests,"App metadata inspection leaves flash/reset untouched");
    std::cout<<"Real F24 App handler / PC metadata readback / reopen tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
