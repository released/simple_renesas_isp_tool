#include "worker_session.h"
#include "core/app_confirmation_timing.h"
#include "core/update_evidence.h"
#include <algorithm>
#include <chrono>

namespace firmware_upgrade::ui {
namespace {
std::uint64_t Elapsed(std::chrono::steady_clock::time_point start) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-start).count());
}
}
bool WorkerSession::UartAction(unsigned action,const isp::BusConfiguration& configuration,
    bool transfer,const std::vector<std::uint8_t>& image,bool force,std::uint32_t approved_version,std::uint8_t contract) {
    if(action>4U||!isp::ValidBus(contract,configuration))return false;
    const CommandType types[]={CommandType::UartDiscover,CommandType::UartEnter,
        CommandType::UartProgram,CommandType::UartTest,CommandType::UartVerify};
    Command command;command.type=types[action];command.uart_configuration=configuration;
    command.uart_transfer=transfer;command.image=image;
    command.isp_contract=contract;
    command.force_old_version=force;command.approved_installed_version=approved_version;
    return Submit(std::move(command));
}
void WorkerSession::AbortUartBestEffort() {
    if(!state_.host_validated||!serial_port_.IsOpen()||
        !(state_.host_capabilities.feature_flags&bridge::kFeatureUart))return;
    const bool old=cleanup_in_progress_;cleanup_in_progress_=true;
    std::wstring error;
    if(!uart_client_.Abort(&error))PostLog(L"Device UART bridge cleanup not confirmed: "+error);
    cleanup_in_progress_=old;uart_client_.Reset();state_.uart_valid=false;
}
bool WorkerSession::ReadUartTarget(std::wstring* error) {
    isp::TargetInfo target;
    if(!isp_client_.Discover(&target,error))return false;
    if(target.contract!=state_.isp_contract) {*error=L"Live device protocol differs from the selected ISP workflow.";return false;}
    if(target.valid) {
        std::vector<std::uint8_t> bytes;
        if(!isp_client_.ReadMetadata(&bytes,error)||!isp::ValidateInstalledMetadata(bytes,target,error))return false;
    }
    state_.uart_target=target;state_.uart_valid=true;
    state_.target_profile=target.mode==ISP_MODE_TRANSFER?TargetProfile::UartTransfer:TargetProfile::UartUpgrade;
    state_.uart_summary=target.family+L" / "+target.part+L" | "+
        (target.mode==ISP_MODE_BOOT?L"Boot":target.mode==ISP_MODE_APP?L"App":L"Custom Transfer")+
        L" | "+isp::BusText(target.contract,state_.uart_configuration)+
        (target.contract==3U?L" / XMODEM-CRC128":L"")+L" | target "+evidence::Hex(target.target);
    state_.target_summary=state_.uart_summary;
    return true;
}
bool WorkerSession::ProcessUart(const Command& command,std::wstring* error) {
    const bool discover=command.type==CommandType::UartDiscover;
    const bool program=command.type==CommandType::UartProgram;
    const bool i2c=command.isp_contract==2U;
    const auto profile=i2c?firmware_update::Profile::I2cUpgrade:command.isp_contract==3U?
        firmware_update::Profile::XmodemUpgrade:firmware_update::Profile::UartUpgrade;
    std::vector<std::uint32_t> counters_before;
    if(!state_.host_validated||!serial_port_.IsOpen()||state_.can_started||
        !(state_.host_capabilities.feature_flags&(i2c?bridge::kFeatureI2c:bridge::kFeatureUart))) {
        *error=L"Device ISP requires a validated bridge with the selected bus capability and CAN stopped.";return false;
    }
    if(!isp::ValidBus(command.isp_contract,command.uart_configuration)) {
        *error=L"Invalid device baud, I2C speed or7-bit address.";return false;
    }
    if(!discover&&(!state_.uart_valid||!isp::SameBus(command.uart_configuration,state_.uart_configuration)||
        command.isp_contract!=state_.isp_contract)) {
        *error=L"Discover the device with the selected port/rate/address first.";return false;
    }
    AbortI2cBestEffort();
    state_.i2c_probe_validated=false;
    AbortUartBestEffort();
    state_.isp_contract=command.isp_contract;
    state_.uart_valid=false;
    state_.target_validated=false;state_.f13_bootloader_ready=false;
    if(i2c) {
        i2c_host::Configuration c;c.port=command.uart_configuration.port;
        c.speed=command.uart_configuration.baud==400000U?1U:0U;
        if(!i2c_client_.Configure(c,state_.i2c_ports,error))return false;
    } else if(!uart_client_.Configure({command.uart_configuration.port,command.uart_configuration.baud},state_.uart_ports,error))return false;
    state_.uart_configuration=command.uart_configuration;
    isp_client_.SetBusConfiguration(command.isp_contract,command.uart_configuration);
    if(program&&!i2c) {
        std::wstring ignored;
        if(!uart_client_.ReadStats(&counters_before,&ignored))PostLog(L"UART1 before counters unavailable: "+ignored);
    }
    bool success=false,end_attempted=false;
    const auto operation=std::chrono::steady_clock::now();
    if(discover) {
        success=ReadUartTarget(error);
        if(success && command.uart_transfer!=(state_.uart_target.mode==ISP_MODE_TRANSFER)) {
            *error=L"Device mode differs from selected UART workflow. Select UART ISP or the standalone Custom Transfer project.";
            success=false;
        }
        if(success)PostLog(L"Live "+state_.uart_summary+L"; installed metadata read only when valid App is reported.");
    } else if(command.type==CommandType::UartTest) {
        success=isp_client_.TransferTest(error);
        if(success) {success=ReadUartTarget(error);PostLog(L"ISP PING/ECHO/counters passed; Xmodem also checks CRC128 streaming. No Flash commands.");}
    } else if(command.type==CommandType::UartVerify) {
        std::vector<std::uint8_t> bytes;
        success=ReadUartTarget(error)&&state_.uart_target.mode==ISP_MODE_BOOT&&state_.uart_target.valid&&
            isp_client_.Request(ISP_CMD_VERIFY,{},&bytes,error,5000U);
        if(success) {
            success=bytes.size()==8U&&image_policy::Read32(bytes.data())==state_.uart_target.size;
            if(success)PostLog(L"Read-only UART VERIFY PASS: bytes="+std::to_wstring(state_.uart_target.size)+
                L", CRC32="+evidence::Hex(image_policy::Read32(bytes.data()+4)));
        }
    } else if(command.type==CommandType::UartEnter) {
        std::vector<std::uint8_t> reply;
        success=ReadUartTarget(error)&&state_.uart_target.mode==ISP_MODE_APP&&
            isp_client_.Request(ISP_CMD_ENTER,{},&reply,error)&&reply.empty();
        if(success) {
            const auto quiet=evidence::DeviceRestartQuietMs(state_.uart_target.size,0U,true);
            PostProgress(0,L"Enter UART Boot / restart wait");
            const auto start=std::chrono::steady_clock::now();
            while(Elapsed(start)<quiet&&!IsCancelRequested())Sleep(20U);
            success=!IsCancelRequested()&&ReadUartTarget(error)&&state_.uart_target.mode==ISP_MODE_BOOT;
            if(success)PostLog(L"ISP Boot confirmed. Start Device Update before the valid-App inactivity window expires.");
        }
    } else if(program) {
        firmware_update::ImageInfo image;std::uint64_t end_ms=0U;
        std::wstring phase;auto phase_started=operation;unsigned last_percent=101U;
        success=firmware_update::ValidateImage(profile,command.image,&image,error);
        if(success)success=isp_client_.Program(command.image,command.force_old_version,command.approved_installed_version,
            [&](unsigned percent,const wchar_t* name) {
                if(phase!=name) {
                    if(!phase.empty())update_timings_.push_back({phase,Elapsed(phase_started)});
                    if(phase==L"4/5 END CRC")end_ms=Elapsed(phase_started);
                    phase=name;phase_started=std::chrono::steady_clock::now();
                }
                if(percent>=92U)end_attempted=true;
                if(percent!=last_percent) {PostProgress(static_cast<int>(percent),name);last_percent=percent;}
            },error);
        if(success) {
            evidence::AppConfirmationTiming timing(evidence::DeviceRestartQuietMs(image.image_size,end_ms));
            const auto start=std::chrono::steady_clock::now();bool confirmed=false;
            PostLog(L"ISP END CRC acknowledged; waiting for APP_READY and installed identity. No automatic rewrite.");
            while(!IsCancelRequested()) {
                const auto action=evidence::AwaitAppProbe(timing,[&](){return Elapsed(start);},
                    [this](){return IsCancelRequested();},[](std::uint32_t n){Sleep(n);});
                if(action!=evidence::AppConfirmationTiming::Action::Probe)break;
                std::wstring probe_error;
                if(ReadUartTarget(&probe_error)&&state_.uart_target.mode==ISP_MODE_APP&&state_.uart_target.ready&&
                    state_.uart_target.valid&&state_.uart_target.version==image.version&&
                    state_.uart_target.build==image.build&&state_.uart_target.size==image.image_size&&
                    state_.uart_target.version_control==(image.policy.version_control!=0U)&&isp::Matches(image,state_.uart_target,&probe_error)) {
                    std::vector<std::uint8_t> metadata;
                    if(isp_client_.ReadMetadata(&metadata,&probe_error)&&
                        std::equal(metadata.begin(),metadata.end(),command.image.begin()+256U)) {confirmed=true;break;}
                }
                if(!probe_error.empty())PostLog(L"ISP App confirmation pending: "+probe_error);
                timing.ProbeFinished(Elapsed(start));
            }
            success=confirmed;
            if(!confirmed)*error=IsCancelRequested()?L"ISP App confirmation canceled; END CRC already succeeded.":
                L"END CRC succeeded, but UART APP_READY/installed identity was not confirmed within the bounded window. No automatic rewrite.";
            else {
                update_result_=feedback::Result::UartAppConfirmed;
                update_details_=std::wstring(firmware_update::DisplayName(profile))+L"; input="+std::to_wstring(image.image_size)+L"; CRC32="+
                    evidence::Hex(image.stored_crc32)+L"; END acknowledged; APP_READY and all256 installed metadata bytes match.";
                PostLog(update_details_);PostProgress(100,L"UART device update complete");
            }
        }
        if(!phase.empty())update_timings_.push_back({phase,Elapsed(phase_started)});
    }
    if(success&&state_.uart_valid&&(state_.uart_target.capabilities&ISP_CAP_DIAGNOSTICS)) {
        isp::UartDiagnostics d;std::wstring reason,details;
        if(isp_client_.ReadDiagnostics(state_.uart_target,&d,&reason)) {
            details=L"RX errors="+std::to_wstring(d.rx_errors)+L"; RX drops="+std::to_wstring(d.rx_drops)+
                L"; TX timeouts="+std::to_wstring(d.tx_timeouts)+L"; RX peak="+std::to_wstring(d.rx_peak)+
                L"; RX pending="+std::to_wstring(d.rx_pending)+L" (device run snapshot; not update deltas)";
        } else details=L"unavailable; "+reason;
        PostLog(L"Device UART observations: "+details);
        if(program)update_details_+=(update_details_.empty()?L"":L"\n")+std::wstring(L"Device UART observations: ")+details;
    }
    if(!success&&!end_attempted&&program) {
        const bool old=cleanup_in_progress_;cleanup_in_progress_=true;
        std::wstring ignored;
        if(i2c)i2c_client_.Abort(&ignored);else uart_client_.Abort(&ignored);
        Sleep(220U);std::vector<std::uint8_t> reply;
        if(command.isp_contract==3U) {
            uart_host::Transaction cancel;cancel.transmit={0x18U,0x18U};cancel.receive_min=0U;cancel.receive_max=0U;
            uart_client_.Transfer(cancel,&reply,&ignored);Sleep(20U);uart_client_.Abort(&ignored);
        }
        isp_client_.Request(ISP_CMD_ABORT,{},&reply,&ignored,500U);
        cleanup_in_progress_=old;
    }
    if(!success||program) {if(i2c){AbortI2cBestEffort();state_.uart_valid=false;}else AbortUartBestEffort();}
    else state_.uart_valid=true;
    if(!success&&error->empty())*error=L"ISP device operation failed its live state or response-length check.";
    if(!success)state_.uart_valid=false;
    if(program&&!i2c) {
        const bool old=cleanup_in_progress_;cleanup_in_progress_=true;
        std::vector<std::uint32_t> after;std::wstring stats_error,details;
        if(uart_client_.ReadStats(&after,&stats_error)&&counters_before.size()==6U&&after.size()==6U) {
            const wchar_t* names[]={L"RX bytes",L"TX bytes",L"RX errors",L"RX drops",L"RX peak",L"backend aborts"};
            bool increased=false;
            for(std::size_t at=0U;at<6U;++at) {
                details+=(details.empty()?L"":L"; ")+std::wstring(names[at])+L"=";
                if(at==4U)details+=std::to_wstring(after[at])+L" (lifetime peak)";
                else if(after[at]==0xFFFFFFFFU||counters_before[at]==0xFFFFFFFFU||after[at]<counters_before[at])details+=L"unavailable (reset/saturated)";
                else {
                    const auto delta=after[at]-counters_before[at];details+=std::to_wstring(delta);
                    if((at==2U||at==3U)&&delta)increased=true;
                }
            }
            if(success&&increased)update_result_=feedback::Result::UartAppConfirmedRxWarning;
        } else details=L"unavailable; "+stats_error;
        PostLog(L"Bridge UART1 observations: "+details);
        update_details_+=(update_details_.empty()?L"":L"\n")+std::wstring(L"UART1 counter deltas: ")+details;
        cleanup_in_progress_=old;
    }
    PostState();return success;
}
} // namespace firmware_upgrade::ui
