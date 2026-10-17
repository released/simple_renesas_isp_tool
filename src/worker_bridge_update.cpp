#include "worker_session.h"
#include "worker_format.h"
#include <cstring>
#include <chrono>
#include "core/update_evidence.h"

namespace firmware_upgrade::ui {
bool WorkerSession::BridgeAction(unsigned action, const std::vector<std::uint8_t>& image) {
    if (action>3U) return false;
    const CommandType kinds[]={CommandType::BridgeInfo,CommandType::BridgeEnter,
        CommandType::BridgeProgram,CommandType::BridgeRun};
    Command command; command.type=kinds[action]; command.image=image;
    return Submit(std::move(command));
}
bool WorkerSession::RefreshBridgeInfo(std::wstring* error) {
    state_.bridge_info_valid=false; state_.bridge_info={};
    if (!(state_.host_capabilities.feature_flags&BU_FEATURE)) return true;
    bu_info_t info={};
    if (!bridge_update::ReadInfo(host_link_client_,&info,error)) return false;
    if (info.board_id!=state_.host_info.board_id ||
        (info.mode==BU_MODE_BOOT && info.uart_baud!=state_.baudrate)) {
        *error=L"Bridge descriptor board / fixed BOOT update UART rate differs from session."; return false;
    }
    state_.bridge_info=info; state_.bridge_info_valid=true;
    host_link::Response diagnostic;
    std::wstring diagnostic_error;
    if(host_link_client_.Request(static_cast<host_link::Command>(BU_DIAGNOSTICS_COMMAND),{},&diagnostic,&diagnostic_error) &&
        diagnostic.payload.size()==8U && diagnostic.payload[0]==1U) {
        const wchar_t* reasons[]={L"none / valid app",L"no valid app",L"metadata invalid",L"image contract mismatch",
            L"CRC mismatch",L"SW1 recovery",L"Flash operation failed",L"ENTER request",L"update complete; explicit RUN required"};
        const auto reason=diagnostic.payload[1];
        PostLog(std::wstring(L"Host Boot stay reason: ")+(reason<9U?reasons[reason]:L"unknown")+L".");
    }else PostLog(L"Host Boot diagnostic unavailable: "+diagnostic_error);
    if (info.flags&BU_FLAG_VALID_APP)
        PostLog(L"HOST BRIDGE metadata read:256 bytes in four64-byte pages;128-byte core and128 reserved bytes verified.");
    PostLog(std::wstring(L"HOST BRIDGE self-update descriptor: ")+
        std::wstring(info.part,info.part+strlen(info.part))+
        (info.mode==BU_MODE_BOOT ? L" BOOT" : L" APP")+
        L", app start "+worker_detail::Hex32(info.app_start)+L".");
    return true;
}
bool WorkerSession::ProcessBridgeAction(const Command& command, std::wstring* error) {
    if (!state_.host_validated || !serial_port_.IsOpen() ||
        !(state_.host_capabilities.feature_flags&BU_FEATURE) || state_.can_started) {
        *error=L"Host bridge self-update requires advertised support and stopped device transports."; return false;
    }
    const auto board=state_.host_info.board_id;
    AbortI2cBestEffort();
    if (!RefreshBridgeInfo(error)) return false;
    if (command.type==CommandType::BridgeInfo) { PostState(); return true; }
    if (command.type==CommandType::BridgeEnter) {
        if (state_.bridge_info.mode!=BU_MODE_APP) { *error=L"Bridge is already in boot mode."; return false; }
        if (!bridge_update::EnterBoot(host_link_client_,board,error)) return false;
        PostLog(L"HOST BRIDGE ENTER acknowledged. Reopening the same COM and revalidating identity; no erase.");
        Command reopen; reopen.type=CommandType::OpenAndProbe;
        reopen.port_name=state_.port_name; reopen.baudrate=state_.bridge_info.uart_baud;
        serial_port_.Close(); ResetSessionState(); PostState();
        Sleep(300U);
        bool connected=false;
        for (unsigned attempt=0U; attempt<8U && !IsCancelRequested(); ++attempt) {
            if (ProcessOpenAndProbe(reopen,error)) { connected=true; break; }
            Sleep(150U);
        }
        if (!connected) return false;
        if (!state_.bridge_info_valid || state_.bridge_info.mode!=BU_MODE_BOOT ||
            state_.host_info.board_id!=board) {
            *error=L"Bridge did not return as the same host in BOOT mode."; return false;
        }
        return true;
    }
    if (command.type==CommandType::BridgeRun) {
        if (state_.bridge_info.mode!=BU_MODE_BOOT || !(state_.bridge_info.flags&BU_FLAG_VALID_APP)) {
            *error=L"RUN requires a CRC-validated installed bridge app."; return false;
        }
        host_link::Response response;
        std::vector<std::uint8_t> request(4U); Bu_Write16(request.data(),board);
        request[2]=BU_PROTOCOL; request[3]=0xA0U;
        const auto expected=state_.bridge_info;
        const auto run_started=std::chrono::steady_clock::now();
        Command reopen; reopen.type=CommandType::OpenAndProbe;
        reopen.port_name=state_.port_name; reopen.baudrate=115200U;
        const bool acknowledged=host_link_client_.RequestWithPolicy(static_cast<host_link::Command>(BU_RUN_COMMAND),
            request,1000U,1U,&response,error);
        update_details_=L"RUN sent once; ack="+std::to_wstring(acknowledged);
        if(!acknowledged) PostLog(L"RUN ACK not confirmed; checking the same host without repeating RUN: "+*error);
        serial_port_.Close(); ResetSessionState(); PostState();
        PostProgress(0,L"1/2 Host reconnect");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
        while(std::chrono::steady_clock::now()<deadline && !IsCancelRequested()) {
            Sleep(150U);
            if(!ProcessOpenAndProbe(reopen,error)) continue;
            const auto& live=state_.bridge_info;
            if(!state_.bridge_info_valid || live.board_id!=board) {
                ProcessDisconnect(); *error=L"RUN reconnect found a different or unverified bridge."; return false;
            }
            if(live.mode!=BU_MODE_APP) continue;
            PostProgress(0,L"2/2 Verify host App");
            if(!evidence::ConfirmBridgeApp(expected,live)) {
                *error=L"RUN returned to APP, but installed metadata/version/build/size differs from the validated Boot descriptor.";
                return false;
            }
            update_details_+=L"; APP confirmed; version="+evidence::Hex(live.app_version)+
                L"; build="+std::to_wstring(live.app_build)+L"; bytes="+std::to_wstring(live.installed_size);
            PostLog(L"HOST BRIDGE RUN confirmation PASS: same board, APP mode and complete installed metadata match.");
            update_timings_.push_back({L"run_reconnect_confirmation",static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-run_started).count())});
            PostProgress(100,L"2/2 Host App confirmed"); *error=L""; return true;
        }
        *error=IsCancelRequested()?L"Host RUN confirmation canceled; no repeated RUN or rewrite.":
            L"Host RUN sent, but APP reconnect was not confirmed within the bounded window. No automatic rewrite.";
        return false;
    }
    if (command.type!=CommandType::BridgeProgram) return false;
    const auto abort=[&]() {
        const bool old=cleanup_in_progress_; cleanup_in_progress_=true;
        host_link::Response response; std::wstring failure;
        if (!host_link_client_.RequestWithPolicy(static_cast<host_link::Command>(BU_ABORT_COMMAND),
            {},1000U,1U,&response,&failure)) PostLog(L"Bridge ABORT not acknowledged: "+failure);
        cleanup_in_progress_=old;
    };
    PostLog(L"Programming HOST BRIDGE APP only over UART. Boot/reserved flash is protected; no auto RUN.");
    const auto image=bridge_update::Inspect(command.image);
    update_details_=L"image_crc32="+evidence::Hex(image.stored_crc)+L"; image_bytes="+std::to_wstring(image.size);
    auto phase_started=std::chrono::steady_clock::now();
    std::wstring phase=L"Preflight";
    const auto finish_phase=[&]() {
        const auto now=std::chrono::steady_clock::now();
        update_timings_.push_back({phase,static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now-phase_started).count())});
        update_details_+=L"; "+phase+L"_ms="+std::to_wstring(std::chrono::duration_cast<std::chrono::milliseconds>(now-phase_started).count());
        phase_started=now;
    };
    const bool result=bridge_update::Program(host_link_client_,state_.bridge_info,command.image,
        [this](){return IsCancelRequested();},
        [this](int n){PostProgress(n,L"3/4 Host transfer");},abort,error,
        [&](unsigned stage) {
            finish_phase(); const wchar_t* names[]={L"1/4 Host metadata",L"2/4 Host BEGIN",L"3/4 Host transfer",L"4/4 Host CRC/readback"};
            phase=names[stage]; PostProgress(0,phase); PostLog(L"Update stage: "+phase);
        });
    finish_phase();
    update_details_+=L"; App execution=not requested (explicit RUN)";
    state_.bridge_info_valid=false;
    if (!IsCancelRequested()) {
        std::wstring refresh_error;
        if (!RefreshBridgeInfo(&refresh_error)) PostLog(L"Bridge INFO refresh failed: "+refresh_error);
    }
    if (result) {
        PostProgress(100,L"4/4 Host CRC/readback");
        PostLog(L"HOST BRIDGE END CRC acknowledged. Remains in boot; RUN is explicit.");
    }
    PostState(); return result;
}
} // namespace firmware_upgrade::ui
