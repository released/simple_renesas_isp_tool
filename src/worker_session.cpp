#include "worker_session.h"
#include "build_info.generated.h"
#include "core/operation_guidance.h"
#include "core/update_report_view.h"
#include "core/update_report_store.h"
#include "worker_format.h"

#include "core/firmware_update_profile.h"
#include "core/host_link_baud.h"
#include "core/update_evidence.h"
#include "core/host_counter_evidence.h"
#include "core/isp_i2c_exchange.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <utility>

namespace firmware_upgrade::ui {
using namespace worker_detail;

WorkerSession::WorkerSession() {
    host_link_client_.SetTransport(&serial_port_);
    host_link_client_.SetCancelCheck(
        [this]() { return IsCancelRequested(); });
    host_link_client_.SetTraceCallback(
        [this](const host_link::TraceRecord& record) {
            PostHostLinkTrace(record);
        });
    can_client_.SetHostLinkClient(&host_link_client_);
    i2c_client_.SetHostLinkClient(&host_link_client_);
    uart_client_.SetHostLinkClient(&host_link_client_);
    isp_client_.SetCancelCheck([this](){return IsCancelRequested();});
    isp_client_.SetExchange([this](const std::vector<std::uint8_t>& bytes,std::uint32_t timeout,
        std::vector<std::uint8_t>* reply,std::wstring* error) {
        if(state_.isp_contract==2U) {
            return isp::MailboxExchange([this](const std::vector<std::uint8_t>& tx,std::uint16_t count,
                std::uint16_t deadline,std::vector<std::uint8_t>* rx,std::wstring* reason) {
                i2c_host::Transaction t;t.address=state_.uart_configuration.address;t.transmit=tx;t.receive_length=count;
                t.timeout_ms=deadline;t.repeated_start=false;return i2c_client_.Transfer(t,rx,reason);
            },[this](){return IsCancelRequested();},bytes,timeout,reply,error);
        }
        uart_host::Transaction transaction;transaction.transmit=bytes;
        transaction.receive_min=14U;transaction.receive_max=ISP_FRAME_MAX;
        transaction.timeout_ms=isp::UartTimeoutMs(state_.uart_configuration.baud,bytes.size(),ISP_FRAME_MAX,timeout);
        transaction.idle_ms=isp::UartIdleMs(state_.uart_configuration.baud);
        return uart_client_.Transfer(transaction,reply,error);
    });
    isp_client_.SetStreamExchange([this](const std::vector<std::uint8_t>& bytes,std::uint32_t timeout,
        std::vector<std::uint8_t>* reply,std::wstring* error) {
        uart_host::Transaction t;t.transmit=bytes;t.receive_min=1U;t.receive_max=1U;
        t.clear_receive=false;t.timeout_ms=isp::UartTimeoutMs(state_.uart_configuration.baud,bytes.size(),1U,timeout);
        const bool success=uart_client_.Transfer(t,reply,error);
        if(!success&&!IsCancelRequested()) {
            const bool old=cleanup_in_progress_;cleanup_in_progress_=true;
            std::wstring ignored;uart_client_.Abort(&ignored);cleanup_in_progress_=old;
        }
        return success;
    });
    can_client_.SetTraceCallback(
        [this](can_host::TraceDirection direction,
               const can_host::Frame& frame) {
            PostCanTrace(direction, frame);
        });
    boot_client_.SetCanClient(&can_client_);
    boot_client_.SetTransactionPolicy(500U, 3000U, 5000U, 1U);
    transfer_client_.SetCanClient(&can_client_);
    transfer_client_.SetTransactionPolicy(500U, 1U);
}

WorkerSession::~WorkerSession() {
    StopAndJoin();
}

bool WorkerSession::Start(HWND notify_window, UINT notify_message) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    if (started_ || notify_window == nullptr || notify_message < WM_APP) {
        return false;
    }

    notify_window_ = notify_window;
    notify_message_ = notify_message;
    stopping_ = false;
    cancel_requested_ = false;
    started_ = true;
    worker_ = std::thread(&WorkerSession::ThreadMain, this);
    return true;
}

void WorkerSession::StopAndJoin() {
    {
        std::lock_guard<std::mutex> lock(command_mutex_);
        if (!started_) {
            return;
        }
        stopping_ = true;
        cancel_requested_ = true;
        commands_.clear();
        commands_.push_back(Command{CommandType::Shutdown, L"", 0U});
        command_pending_or_busy_ = true;
    }
    command_ready_.notify_one();

    if (worker_.joinable()) {
        worker_.join();
    }

    {
        std::lock_guard<std::mutex> lock(command_mutex_);
        started_ = false;
        command_pending_or_busy_ = false;
        commands_.clear();
        notify_window_ = nullptr;
        notify_message_ = 0U;
    }
}

bool WorkerSession::OpenAndProbe(const std::wstring& port_name,
                                 std::uint32_t baudrate) {
    if (port_name.empty() || baudrate == 0U) {
        return false;
    }
    return Submit(Command{CommandType::OpenAndProbe, port_name, baudrate});
}

bool WorkerSession::ConnectF13Transfer(const CanSettings& settings) {
    Command command;
    command.type = CommandType::ConnectF13Transfer;
    command.can_settings = settings;
    return Submit(std::move(command));
}

bool WorkerSession::ApplyCanSettings(const CanSettings& settings) {
    Command command;
    command.type = CommandType::ApplyCanSettings;
    command.can_settings = settings;
    return Submit(std::move(command));
}

bool WorkerSession::RunF13TransferTest() {
    return Submit(Command{CommandType::RunF13TransferTest, L"", 0U});
}

bool WorkerSession::DiscoverF13Boot(const CanSettings& settings) {
    Command command;
    command.type = CommandType::DiscoverF13Boot;
    command.can_settings = settings;
    return Submit(std::move(command));
}

bool WorkerSession::EnterF13Bootloader(const CanSettings& settings) {
    Command command;
    command.type = CommandType::EnterF13Bootloader;
    command.can_settings = settings;
    return Submit(std::move(command));
}

bool WorkerSession::ProgramF13Boot(
    const std::vector<std::uint8_t>& image,bool force_old_version,
    std::uint32_t approved_installed_version) {
    Command command;
    command.type = CommandType::ProgramF13Boot;
    command.image = image;
    command.force_old_version=force_old_version;
    command.approved_installed_version=approved_installed_version;
    return Submit(std::move(command));
}

bool WorkerSession::StartCanTxTest(const CanSettings& settings) {
    Command command;
    command.type = CommandType::CanTxTest;
    command.can_settings = settings;
    can_tx_test_stop_requested_ = false;
    return Submit(std::move(command));
}

bool WorkerSession::StopCanTxTest() {
    can_tx_test_stop_requested_ = true;
    return true;
}

bool WorkerSession::ResetHost() {
    return Submit(Command{CommandType::ResetHost, L"", 0U});
}

bool WorkerSession::RequestDisconnect() {
    {
        std::lock_guard<std::mutex> lock(command_mutex_);
        if (!started_ || stopping_) {
            return false;
        }
        cancel_requested_ = true;
        const bool already_queued = std::any_of(
            commands_.begin(), commands_.end(),
            [](const Command& command) {
                return command.type == CommandType::Disconnect;
            });
        if (!already_queued) {
            commands_.push_back(Command{CommandType::Disconnect, L"", 0U});
        }
        command_pending_or_busy_ = true;
    }
    command_ready_.notify_one();
    return true;
}

bool WorkerSession::RequestCancel() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    if (!started_ || stopping_ || !command_pending_or_busy_) return false;
    cancel_requested_=true;
    can_tx_test_stop_requested_=true;
    return true; // The worker retains ownership of COM and performs bounded cleanup.
}

bool WorkerSession::VerifyInstalled() {
    return Submit(Command{CommandType::VerifyInstalled,L"",0U});
}

void WorkerSession::SetDetailedTrace(bool enabled) {
    detailed_trace_enabled_.store(enabled);
}

std::vector<WorkerEvent> WorkerSession::DrainEvents(std::size_t maximum) {
    std::vector<WorkerEvent> drained;
    std::lock_guard<std::mutex> lock(event_mutex_);
    if(maximum==0U) return drained;
    const auto dropped=events_.TakeDropped();
    if (dropped != 0U) {
        WorkerEvent warning;
        warning.kind = WorkerEventKind::Log;
        warning.text = L"WARNING: UI event queue overflow; " + std::to_wstring(dropped) +
            L" events/log lines dropped. This log is incomplete; do not count trace lines as wire packets.";
        drained.push_back(std::move(warning));
    }
    auto batch=events_.Take(maximum-drained.size());
    for(auto& event:batch) drained.push_back(std::move(event));
    return drained;
}

bool WorkerSession::HasPendingEvents() {
    std::lock_guard<std::mutex> lock(event_mutex_);
    return events_.Pending();
}

bool WorkerSession::Submit(Command command) {
    {
        std::lock_guard<std::mutex> lock(command_mutex_);
        if (!started_ || stopping_ || command_pending_or_busy_) {
            return false;
        }
        cancel_requested_ = false;
        commands_.push_back(std::move(command));
        command_pending_or_busy_ = true;
    }
    command_ready_.notify_one();
    return true;
}

void WorkerSession::ThreadMain() {
    for (;;) {
        Command command;
        bool have_command = false;
        {
            std::unique_lock<std::mutex> lock(command_mutex_);
            command_ready_.wait_for(lock, std::chrono::milliseconds(25),
                                    [this]() {
                return !commands_.empty();
            });
            if (!commands_.empty()) {
                command = std::move(commands_.front());
                commands_.pop_front();
                have_command = true;
            }
        }

        if (!have_command) {
            PollHostEvents();
            continue;
        }

        if (command.type == CommandType::Shutdown) {
            break;
        }

        state_.busy = true;
        PostState();
        std::wstring error;
        bool success = false;
        const bool uart_update=command.type==CommandType::UartProgram;
        const bool report_update=uart_update || command.type==CommandType::ProgramF13Boot ||
            command.type==CommandType::BridgeProgram || command.type==CommandType::BridgeRun;
        const auto report_started=std::chrono::steady_clock::now();
        const auto report_port=state_.port_name;
        const auto report_baud=state_.baudrate;
        const auto report_board=state_.host_info.board_id;
        if(report_update) RefreshHostStatsBestEffort();
        const bool stats_before_valid=state_.f24_stats_valid;
        const auto stats_before=state_.f24_stats;
        const auto client_before=host_link_client_.Stats();
        update_details_.clear();
        update_result_=feedback::Result::None;
        current_phase_.clear();
        update_timings_.clear();
        const auto report_host=state_.host_summary;
        const auto report_target=state_.target_summary;
        const auto report_can=uart_update ? state_.uart_summary : state_.can_summary;
        const std::wstring isp_name=command.isp_contract==2U?L"I2C ISP":command.isp_contract==3U?L"XMODEM-CRC128":L"UART ISP";

        try {
            switch (command.type) {
                case CommandType::UartDiscover:
                case CommandType::UartEnter:
                case CommandType::UartProgram:
                case CommandType::UartTest:
                case CommandType::UartVerify:
                    success=ProcessUart(command,&error);
                    break;
                case CommandType::OpenAndProbe:
                    success = ProcessOpenAndProbe(command, &error);
                    break;
                case CommandType::ApplyCanSettings:
                    success = ProcessApplyCanSettings(command, &error);
                    break;
                case CommandType::ConnectF13Transfer:
                    success = ProcessConnectF13Transfer(command, &error);
                    break;
                case CommandType::RunF13TransferTest:
                    success = ProcessRunF13TransferTest(&error);
                    break;
                case CommandType::DiscoverF13Boot:
                    success = ProcessDiscoverF13Boot(command, &error);
                    break;
                case CommandType::EnterF13Bootloader:
                    success = ProcessEnterF13Bootloader(command, &error);
                    break;
                case CommandType::ProgramF13Boot:
                    success = ProcessProgramF13Boot(command, &error);
                    break;
                case CommandType::VerifyInstalled: {
                    f13_boot::Ack ack;
                    const auto diagnostic_flags=state_.target_metadata.diagnostics;
                    success=state_.f13_bootloader_ready && (state_.target_metadata.diagnostics&2U) &&
                        boot_client_.GetTargetInfo(&state_.target_metadata,&error) && boot_client_.VerifyInstalled(&ack,&error);
                    state_.target_metadata.diagnostics=diagnostic_flags;
                    if(success) PostLog(L"Read-only VERIFY PASS: range start="+evidence::Hex(state_.target_metadata.application_start)+
                        L", installed bytes="+std::to_wstring(state_.target_metadata.installed_app_size)+
                        L", recalculated CRC32="+evidence::Hex(ack.value)+L". No erase/write.");
                    break;
                }
                case CommandType::CanTxTest:
                    success = ProcessCanTxTest(command, &error);
                    break;
                case CommandType::ResetHost:
                    success = ProcessResetHost(&error);
                    break;
                case CommandType::ProbeI2c:
                    success = ProcessProbeI2c(command, &error);
                    break;
                case CommandType::BridgeInfo:
                case CommandType::BridgeEnter:
                case CommandType::BridgeProgram:
                case CommandType::BridgeRun:
                    success = ProcessBridgeAction(command, &error);
                    break;
                case CommandType::Disconnect:
                    cancel_requested_ = false;
                    ProcessDisconnect();
                    success = true;
                    break;
                case CommandType::Shutdown:
                    break;
            }
        } catch (const std::exception&) {
            error = L"Worker operation failed with an unexpected exception.";
            success = false;
        } catch (...) {
            error = L"Worker operation failed with an unknown exception.";
            success = false;
        }

        if(!success && IsCancelRequested()) error=L"Operation canceled. "+error;
        if(!success && error.empty()) error=L"Worker operation failed.";
        const auto error_context=std::wstring(CommandName(command.type))+L" | "+
            (report_port.empty()?command.port_name:report_port)+L" | Host Baud "+
            std::to_wstring(report_baud?report_baud:command.baudrate)+
            (current_phase_.empty()?L"":L" | Phase: "+current_phase_);
        auto result_message=success && update_result_!=feedback::Result::None ?
            feedback::Describe(update_result_).status : error;
        const auto help=success?std::wstring():guidance::Format(error,error_context);
        state_.busy = false;
        if(report_update) {
            const bool old_cleanup=cleanup_in_progress_; cleanup_in_progress_=true;
            RefreshHostStatsBestEffort(); cleanup_in_progress_=old_cleanup;
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-report_started).count();
            const auto& after=state_.f24_stats;
            const bool comparable=stats_before_valid && state_.f24_stats_valid &&
                command.type==CommandType::ProgramF13Boot;
            const auto observations=evidence::ObserveHostCounters(stats_before,after,comparable);
            const auto& counters=observations.deltas;
            PostLog(L"Update counter deltas: "+counters);
            PostLog(L"Host counter assessment: "+observations.Status()+L". "+observations.Note());
            if(success && observations.increased && update_result_==feedback::Result::DeviceAppConfirmed) {
                update_result_=feedback::WithHostCounterWarning(update_result_,observations.increased);
                result_message=feedback::Describe(update_result_).status;
            }
            const auto report_result=feedback::ReportResult(success,IsCancelRequested(),update_result_);
            const std::wstring peaks=state_.f24_stats_valid && after.telemetry_available ?
                L"since host boot/CLEAR_STATS: UART RX bytes="+std::to_wstring(after.uart_rx_high_water)+
                L"; UART TX bytes="+std::to_wstring(after.uart_tx_high_water)+
                L"; CAN RX frames="+std::to_wstring(after.can_rx_high_water)+
                L"; pending transactions="+std::to_wstring(after.transaction_queue_high_water)+
                L"; max queue wait ms="+std::to_wstring(after.queue_wait_max_ms)+
                L"; max transaction ms="+std::to_wstring(after.transaction_max_ms)+
                L"; max dispatcher call ms="+std::to_wstring(after.process_max_ms):L"unavailable";
            PostLog(L"Host high-water / latency: "+peaks);
            WorkerEvent report; report.kind=WorkerEventKind::UpdateReport;
            std::wstring phases=L"{";
            for(const auto& phase:update_timings_) {
                if(phases.size()>1U) phases+=L",";
                phases+=evidence::JsonString(phase.first)+L":"+std::to_wstring(phase.second);
            }
            phases+=L"}";
            report.text=L"{\n  \"schema\": 1,\n  \"operation\": "+evidence::JsonString(
                uart_update ? isp_name+L" device update" : command.type==CommandType::ProgramF13Boot ? L"CAN device update" :
                command.type==CommandType::BridgeRun ? L"Host explicit RUN confirmation" : L"Host app update")+
                L",\n  \"result\": "+evidence::JsonString(report_result)+
                L",\n  \"message\": "+evidence::JsonString(result_message)+
                L",\n  \"port\": "+evidence::JsonString(report_port)+L",\n  \"host_board\": "+std::to_wstring(report_board)+
                L",\n  \"host_baud\": "+std::to_wstring(report_baud)+
                L",\n  \"host\": "+evidence::JsonString(report_host)+
                L",\n  \"target_before\": "+evidence::JsonString(report_target)+
                L",\n  \"can\": "+evidence::JsonString(uart_update?L"":report_can)+
                L",\n  \"device_uart\": "+evidence::JsonString(uart_update&&command.isp_contract!=2U?report_can:L"")+
                L",\n  \"device_i2c\": "+evidence::JsonString(uart_update&&command.isp_contract==2U?report_can:L"")+
                L",\n  \"device_protocol\": "+evidence::JsonString(uart_update?isp_name:L"")+
                L",\n  \"elapsed_ms\": "+std::to_wstring(elapsed)+
                L",\n  \"input_bytes\": "+std::to_wstring(command.image.size())+
                L",\n  \"phases_ms\": "+phases+
                L",\n  \"details\": "+evidence::JsonString(update_details_)+
                L",\n  \"host_deltas\": "+evidence::JsonString(counters)+
                L",\n  \"host_counter_assessment\": "+evidence::JsonString(observations.Status())+
                L",\n  \"host_counter_note\": "+evidence::JsonString(observations.Note())+
                L",\n  \"host_peaks\": "+evidence::JsonString(peaks)+
                L",\n  \"link_retries\": "+(command.type==CommandType::BridgeRun ||
                    host_link_client_.Stats().retries<client_before.retries ? std::wstring(L"null") :
                    std::to_wstring(host_link_client_.Stats().retries-client_before.retries))+L"\n}\n";
            // Render and flush on the worker, never on the window-message path.
            try {
                evidence::ReportView view;
                view.operation=uart_update?isp_name+L" device update":command.type==CommandType::ProgramF13Boot?L"CAN device update":
                    command.type==CommandType::BridgeRun?L"Host explicit RUN confirmation":L"Host app update";
                view.result=report_result;
                view.message=result_message; view.guidance=help;
                view.tool=FIRMWARE_UPGRADE_TOOL_VERSION; view.build=FIRMWARE_UPGRADE_TOOL_BUILD_TIME;
                SYSTEMTIME utc={}; GetSystemTime(&utc);
                wchar_t stamp[40]={}; swprintf_s(stamp,L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
                    utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds);
                view.recorded_utc=stamp; view.phases=update_timings_;
                view.fields={{L"COM",report_port},{L"Host Baud",std::to_wstring(report_baud)},
                    {L"Host",report_host},{L"Target before operation",report_target},{uart_update?L"Device bus configuration":L"CAN configuration",report_can},
                    {L"Input bytes",std::to_wstring(command.image.size())},{L"Elapsed ms",std::to_wstring(elapsed)},
                    {L"Operation evidence",update_details_},{L"Host counter deltas",counters},
                    {L"Host counter assessment",observations.Status()+L". "+observations.Note()},{L"Host peaks / scope",peaks}};
                if(command.type!=CommandType::BridgeRun && !command.image.empty()) {
                    if(command.type==CommandType::ProgramF13Boot || uart_update) {
                        const auto input=firmware_update::InspectImage(command.image);
                        view.fields.push_back({L"Offline BIN CRC32 (calculated / stored)",evidence::Hex(input.calculated_crc32)+L" / "+evidence::Hex(input.stored_crc32)});
                        if(input.metadata_valid) view.fields.push_back({L"Offline BIN identity",input.metadata.mcu_family+L" / "+input.metadata.mcu_part+
                            L"; version="+evidence::Hex(input.metadata.version)+L"; build="+std::to_wstring(input.metadata.build)});
                    } else {
                        const auto input=bridge_update::Inspect(command.image);
                        view.fields.push_back({L"Offline BIN CRC32 (calculated / stored)",evidence::Hex(input.calculated_crc)+L" / "+evidence::Hex(input.stored_crc)});
                        if(input.valid) view.fields.push_back({L"Offline HOST BIN identity",L"board="+evidence::Hex(input.metadata.board_id)+
                            L"; version="+evidence::Hex(input.metadata.version)+L"; build="+std::to_wstring(input.metadata.build)});
                    }
                }
                const auto document=L"{\n  \"tool\": "+evidence::JsonString(view.tool)+
                    L",\n  \"build\": "+evidence::JsonString(view.build)+L",\n  \"recorded_at_utc\": "+
                    evidence::JsonString(view.recorded_utc)+L",\n  \"failure_guidance\": "+
                    evidence::JsonString(help)+L",\n  \"update\": "+report.text+L"}\n";
                const auto saved=evidence::SaveReportPair(document,evidence::RenderReport(view));
                report.text=saved.error; report.report_json_path=saved.json_path; report.report_html_path=saved.html_path;
            } catch(...) {report.text=L"Report generation failed; use Export Summary.";}
            QueueEvent(std::move(report));
        }
        PostState();
        if (command.type != CommandType::Disconnect) {
            if (!success && error.empty()) {
                error = L"Worker operation failed.";
            }
            WorkerEvent completion; completion.kind=WorkerEventKind::OperationComplete;
            completion.success=success; completion.text=success?L"Operation completed.":error;
            completion.update_result=success ? update_result_ : feedback::Result::None;
            if (completion.update_result!=feedback::Result::None)
                completion.text=feedback::Describe(completion.update_result).status;
            completion.guidance=help; QueueEvent(std::move(completion));
        }

        {
            std::lock_guard<std::mutex> lock(command_mutex_);
            command_pending_or_busy_ = !commands_.empty();
            if (commands_.empty()) cancel_requested_=false;
        }
    }

    cancel_requested_ = false;
    AbortUartBestEffort();
    StopCanBestEffort();
    RestoreLinkBaudBestEffort();
    serial_port_.Close();
    ResetSessionState();
}

bool WorkerSession::ProcessOpenAndProbe(const Command& command,
                                        std::wstring* error) {
    ProcessDisconnect();
    if (IsCancelRequested()) {
        *error = L"Open canceled.";
        return false;
    }

    host_link::HostInfo host_info;
    host_link::Capabilities capabilities;
    PostLog(L"Probing MCU host identity and Host Link v2 capabilities.");
    if (!OpenHostLink(command.port_name,command.baudrate,&host_info,&capabilities,error)) {
        serial_port_.Close();
        ResetSessionState();
        PostState();
        return false;
    }
    state_.serial_open = true;
    state_.port_name = command.port_name;

    std::wstringstream summary;
    summary << L"Bridge " << Hex16(host_info.board_id)
            << L"  FW "
            << static_cast<unsigned>(host_info.firmware_major) << L"."
            << static_cast<unsigned>(host_info.firmware_minor) << L"."
            << static_cast<unsigned>(host_info.firmware_patch)
            << L" build " << host_info.firmware_build
            << L"  Host Link v2 / " << state_.baudrate << L" baud";
    state_.host_validated = true;
    state_.host_summary = summary.str();
    state_.host_info = host_info;
    state_.host_capabilities = capabilities;
    if (!RefreshBridgePorts(error) || !RefreshBridgeInfo(error)) {
        ProcessDisconnect();
        return false;
    }
    PostLog(L"MCU host validated: " + state_.host_summary);
    PostLog(L"Capabilities: payload=" +
            std::to_wstring(capabilities.maximum_host_link_payload) +
            L", CAN bytes=" +
            std::to_wstring(capabilities.maximum_can_data_bytes) +
            L", filters=" + std::to_wstring(capabilities.filter_slots) + L".");
    RefreshHostStatsBestEffort();
    PostState();
    return true;
}

bool WorkerSession::ProcessResetHost(std::wstring* error) {
    std::wstring port_name;
    std::uint32_t baudrate;
    std::wstring reset_error;
    host_link::HostInfo host_info;
    host_link::Capabilities capabilities;
    bool acknowledged;

    if (!state_.host_validated || !serial_port_.IsOpen()) {
        *error = L"MCU host is not validated.";
        return false;
    }

    const auto original_board = state_.host_info.board_id;
    port_name = state_.port_name;
    baudrate = state_.requested_baudrate;
    AbortI2cBestEffort();
    AbortUartBestEffort();
    StopCanBestEffort();
    PostLog(L"Sending one-shot RESET_HOST; waiting for ACK before the bridge "
            L"software reset.");
    acknowledged = host_link_client_.ResetHost(&reset_error);
    if (!acknowledged) {
        PostLog(L"RESET_HOST ACK warning: " + reset_error +
                L" Reconnect probing will determine the final result.");
    }

    serial_port_.Close();
    ResetSessionState();
    PostState();
    Sleep(250U);

    for (unsigned attempt = 1U; attempt <= 2U; ++attempt) {
        std::wstring open_error;
        std::wstring probe_error;

        if (IsCancelRequested()) {
            serial_port_.Close();
            ResetSessionState();
            PostState();
            *error = L"Host reset reconnect canceled.";
            return false;
        }
        if (OpenHostLink(port_name,baudrate,&host_info,&capabilities,&open_error)) {
            if (host_info.board_id!=original_board) {
                serial_port_.Close(); ResetSessionState(); PostState();
                *error=L"Reset reconnect found a different bridge board ID."; return false;
            }
            {
                std::wstringstream summary;
                summary << L"Bridge " << Hex16(host_info.board_id)
                        << L"  FW "
                        << static_cast<unsigned>(host_info.firmware_major)
                        << L"."
                        << static_cast<unsigned>(host_info.firmware_minor)
                        << L"."
                        << static_cast<unsigned>(host_info.firmware_patch)
                        << L" build " << host_info.firmware_build
                        << L"  Host Link v2 / " << state_.baudrate << L" baud";
                state_.serial_open = true;
                state_.host_validated = true;
                state_.port_name = port_name;
                state_.host_summary = summary.str();
                state_.host_info = host_info;
                state_.host_capabilities = capabilities;
                if (!RefreshBridgePorts(&probe_error) || !RefreshBridgeInfo(&probe_error)) {
                    serial_port_.Close();
                    continue;
                }
                PostLog(L"Bridge reset/reconnect complete after probe attempt " +
                        std::to_wstring(attempt) + L": " +
                        state_.host_summary);
                RefreshHostStatsBestEffort();
                PostState();
                return true;
            }
        }
        Sleep(100U);
    }

    serial_port_.Close();
    ResetSessionState();
    PostState();
    *error = acknowledged ?
        L"Bridge acknowledged reset but bounded bootstrap/baud reconnect failed." :
        L"RESET_HOST was not acknowledged and bounded reconnect failed.";
    return false;
}

void WorkerSession::ProcessDisconnect() {
    const bool was_busy = state_.busy;
    cancel_requested_ = false;
    AbortI2cBestEffort();
    AbortUartBestEffort();
    StopCanBestEffort();
    RestoreLinkBaudBestEffort();
    if (state_.f24_stats_valid) {
        PostState();
    }
    if (serial_port_.IsOpen()) {
        PostLog(L"COM closed: " + state_.port_name + L".");
        serial_port_.Close();
    }
    ResetSessionState();
    state_.busy = was_busy;
    PostState();
}

void WorkerSession::StopCanBestEffort() {
    if (!state_.host_validated || !serial_port_.IsOpen()) {
        state_.can_started = false;
        state_.target_validated = false;
        state_.f13_bootloader_ready = false;
        state_.target_profile = TargetProfile::None;
        state_.target_metadata = {};
        state_.target_summary.clear();
        return;
    }

    if (state_.can_started) {
        std::uint32_t controller_state = 0U;
        std::wstring error;
        if (!can_client_.Stop(&controller_state, &error)) {
            PostLog(L"CAN stop warning: " + error);
        } else {
            PostLog(L"CAN stopped, controller state " +
                    Hex32(controller_state) + L".");
        }
    }
    RefreshHostStatsBestEffort();
    state_.can_started = false;
    state_.target_validated = false;
    state_.f13_bootloader_ready = false;
    state_.target_profile = TargetProfile::None;
    state_.target_metadata = {};
    state_.target_summary.clear();
}

void WorkerSession::RefreshHostStatsBestEffort() {
    host_link::HostStats stats;
    std::wstring error;

    state_.f24_stats_valid=false;
    if (!state_.host_validated || !serial_port_.IsOpen()) {
        return;
    }
    if (!host_link_client_.GetStats(&stats, &error)) {
        PostLog(L"Host statistics warning: " + error);
        return;
    }
    state_.f24_stats = stats;
    state_.f24_stats_valid = true;
    PostLog(L"F24 counters: CAN RX=" +
            std::to_wstring(stats.can_rx_frames) + L", TX=" +
            std::to_wstring(stats.can_tx_frames) + L", lost=" +
            std::to_wstring(stats.can_rx_lost_frames) + L", TX failures=" +
            std::to_wstring(stats.can_tx_failures) + L", warning=" +
            std::to_wstring(stats.can_warning_entries) + L", passive=" +
            std::to_wstring(stats.can_error_passive_entries) + L", bus-off=" +
            std::to_wstring(stats.can_bus_off_entries) + L", recovery=" +
            std::to_wstring(stats.can_recovery_count) + L".");
}

void WorkerSession::PollHostEvents() {
    std::wstring error;
    host_link::Frame event;
    bool handled = false;

    if (!state_.host_validated || !serial_port_.IsOpen() || state_.busy) {
        return;
    }
    while (host_link_client_.TryPopEvent(&event)) {
        HandleHostLinkEvent(event);
        handled = true;
    }
    if (!host_link_client_.PollEvents(10U, &error)) {
        PostLog(L"Host Link background read failed: " + error);
        serial_port_.Close();
        ResetSessionState();
        PostState();
        return;
    }
    while (host_link_client_.TryPopEvent(&event)) {
        HandleHostLinkEvent(event);
        handled = true;
    }
    if (handled) {
        PostState();
    }
}

void WorkerSession::HandleHostLinkEvent(const host_link::Frame& event) {
    const auto command = static_cast<host_link::Command>(event.command);

    if (command == host_link::Command::CanRxEvent) {
        if (event.payload.size() < 12U || event.payload[0] != 0U ||
            event.payload.size() !=
                12U + static_cast<std::size_t>(event.payload[3])) {
            PostLog(L"Invalid asynchronous CAN_RX_EVENT payload ignored.");
            return;
        }
        const std::uint32_t id = ReadLe32(&event.payload[4]);
        const std::uint32_t timestamp = ReadLe32(&event.payload[8]);
        PostLog(L"Asynchronous CAN RX: ID=" + Hex32(id) +
                L", DLC=" + std::to_wstring(event.payload[2]) +
                L", timestamp=" + std::to_wstring(timestamp) + L".");
        return;
    }

    if (command == host_link::Command::CanStateEvent ||
        command == host_link::Command::HostErrorEvent) {
        if (event.payload.size() != 12U) {
            PostLog(L"Invalid asynchronous CAN diagnostic payload ignored.");
            return;
        }
        const std::uint16_t channel_sources = ReadLe16(&event.payload[0]);
        const std::uint16_t global_sources = ReadLe16(&event.payload[2]);
        const std::uint32_t controller_state = ReadLe32(&event.payload[4]);
        const std::uint32_t arbitration_lost = ReadLe32(&event.payload[8]);
        state_.arbitration_lost_entries = arbitration_lost;
        PostLog(std::wstring(command == host_link::Command::CanStateEvent ?
                                L"CAN state event: " :
                                L"CAN error event: ") +
                L"channel_sources=" + Hex16(channel_sources) +
                L" [" + CanChannelSourcesText(channel_sources) + L"]" +
                L", global_sources=" + Hex16(global_sources) +
                L" [" + CanGlobalSourcesText(global_sources) + L"]" +
                L", controller_state=" + Hex32(controller_state) +
                L", arbitration_lost=" +
                std::to_wstring(arbitration_lost) + L".");
        if ((channel_sources &
             host_link::kCanChannelSourceArbitrationLost) != 0U) {
            PostLog(L"CAN arbitration loss diagnosed; the frame was not the "
                    L"bus winner. Cumulative count=" +
                    std::to_wstring(arbitration_lost) + L".");
        }
        return;
    }

    PostLog(L"Unknown Host Link event 0x" +
            HexBytes({event.command}) + L" ignored.");
}

void WorkerSession::ResetSessionState() {
    const bool was_busy = state_.busy;
    state_ = {};
    state_.busy = was_busy;
}

bool WorkerSession::IsCancelRequested() const {
    return cancel_requested_.load() && !cleanup_in_progress_;
}

void WorkerSession::PostLog(const std::wstring& text) {
    WorkerEvent event;
    event.kind = WorkerEventKind::Log;
    event.text = text;
    QueueEvent(std::move(event));
}

void WorkerSession::PostState() {
    WorkerEvent event;
    const host_link::ClientStats& stats = host_link_client_.Stats();
    event.kind = WorkerEventKind::State;
    event.state = state_;
    event.state.host_requests = stats.requests;
    event.state.host_retries = stats.retries;
    event.state.host_timeouts = stats.response_timeouts;
    event.state.unmatched_frames = stats.unmatched_frames;
    event.state.received_events = stats.received_events;
    event.state.dropped_events = stats.dropped_events;
    QueueEvent(std::move(event));
}

void WorkerSession::PostHostLinkTrace(
    const host_link::TraceRecord& record) {
    if (!detailed_trace_enabled_.load()) {
        return;
    }

    const wchar_t* kind = L"UART";
    switch (record.kind) {
        case host_link::TraceKind::Request: kind = L"UART TX"; break;
        case host_link::TraceKind::Response: kind = L"UART RX"; break;
        case host_link::TraceKind::Event: kind = L"UART EVENT"; break;
        case host_link::TraceKind::Retry: kind = L"UART RETRY"; break;
        case host_link::TraceKind::Timeout: kind = L"UART TIMEOUT"; break;
        case host_link::TraceKind::Unmatched: kind = L"UART UNMATCHED"; break;
    }

    std::wstringstream stream;
    stream << L"TRACE " << kind << L" seq=" << record.sequence
           << L" cmd=0x" << std::hex << std::uppercase
           << std::setfill(L'0') << std::setw(2)
           << static_cast<unsigned>(record.command)
           << L" status=0x" << std::setw(2)
           << static_cast<unsigned>(record.status);
    if (record.attempt != 0U) {
        stream << std::dec << L" attempt=" << record.attempt;
    }
    stream << L" payload=[" << HexBytes(record.payload) << L"]";
    PostLog(stream.str());
}

void WorkerSession::PostCanTrace(can_host::TraceDirection direction,
                                 const can_host::Frame& frame) {
    if (!detailed_trace_enabled_.load()) {
        return;
    }

    std::wstringstream stream;
    stream << L"TRACE CAN "
           << (direction == can_host::TraceDirection::Transmit ? L"TX" : L"RX")
           << L" id=0x" << std::hex << std::uppercase
           << std::setfill(L'0') << std::setw(frame.extended ? 8 : 3)
           << frame.id << std::dec
           << L" IDE=" << (frame.extended ? 1 : 0)
           << L" RTR=" << (frame.remote ? 1 : 0)
           << L" FDF=" << (frame.fd ? 1 : 0)
           << L" BRS=" << (frame.bit_rate_switch ? 1 : 0)
           << L" DLC=" << static_cast<unsigned>(frame.dlc)
           << L" timestamp=" << frame.timestamp
           << L" data=[" << HexBytes(frame.data) << L"]";
    if ((frame.id == f13_boot::kRequestId ||
         frame.id == f13_boot::kResponseId) && !frame.data.empty()) {
        const unsigned command = frame.data[0];
        stream << L" decoded=F13_BOOT command=0x" << std::hex
               << std::setw(2) << command << std::dec;
        if (frame.id == f13_boot::kResponseId && frame.data.size() >= 2U) {
            stream << L" status=" << static_cast<unsigned>(frame.data[1]);
        }
        if (frame.data.size() >= 8U) {
            if (command == static_cast<unsigned>(f13_boot::Command::Info)) {
                stream << L" page=" << static_cast<unsigned>(frame.data[2])
                       << L" state=" << static_cast<unsigned>(frame.data[3]);
            } else if (frame.id == f13_boot::kResponseId) {
                const unsigned sequence =
                    static_cast<unsigned>(frame.data[2]) |
                    (static_cast<unsigned>(frame.data[3]) << 8U);
                stream << L" sequence=" << sequence;
            } else if (command == static_cast<unsigned>(f13_boot::Command::BlockBegin) ||
                       command == static_cast<unsigned>(f13_boot::Command::BlockCommit)) {
                const unsigned offset =
                    static_cast<unsigned>(frame.data[1]) |
                    (static_cast<unsigned>(frame.data[2]) << 8U) |
                    (static_cast<unsigned>(frame.data[3]) << 16U);
                stream << L" block_offset=" << offset;
            }
            if (frame.id == f13_boot::kResponseId) {
                const std::uint32_t value =
                    static_cast<std::uint32_t>(frame.data[4]) |
                    (static_cast<std::uint32_t>(frame.data[5]) << 8U) |
                    (static_cast<std::uint32_t>(frame.data[6]) << 16U) |
                    (static_cast<std::uint32_t>(frame.data[7]) << 24U);
                stream << L" value=" << Hex32(value);
            }
        }
    } else if ((frame.id == f13_transfer::kRequestId ||
                frame.id == f13_transfer::kResponseId) &&
               frame.data.size() == f13_transfer::kFrameSize) {
        const bool crc_ok = f13_transfer::Crc8Atm(
                                frame.data.data(),
                                f13_transfer::kFrameSize - 1U) ==
                            frame.data[f13_transfer::kFrameSize - 1U];
        stream << L" decoded=CUSTOM_TRANSFER sequence="
               << static_cast<unsigned>(frame.data[2])
               << L" command=0x" << std::hex << std::setw(2)
               << static_cast<unsigned>(frame.data[3]) << std::dec
               << L" crc=" << (crc_ok ? L"OK" : L"BAD");
        if (frame.data[3] == static_cast<std::uint8_t>(
                                  f13_transfer::Command::ErrorResponse)) {
            stream << L" rejected_command=0x" << std::hex << std::setw(2)
                   << static_cast<unsigned>(frame.data[4]) << std::dec
                   << L" status=" << static_cast<unsigned>(frame.data[5])
                   << L" detail=" << static_cast<unsigned>(frame.data[6]);
        }
    }
    PostLog(stream.str());
}

void WorkerSession::PostProgress(int progress, const std::wstring& text) {
    current_phase_=text;
    WorkerEvent event;
    event.kind = WorkerEventKind::Progress;
    event.progress = (std::max)(0, (std::min)(progress, 100));
    event.text = text;
    QueueEvent(std::move(event));
}

void WorkerSession::PostActivity(const std::wstring& text) {
    current_phase_=text;
    WorkerEvent event;
    event.kind=WorkerEventKind::Progress;
    event.indeterminate=true;
    event.text=text;
    QueueEvent(std::move(event));
}

const wchar_t* WorkerSession::CommandName(CommandType type) {
    switch(type) {
    case CommandType::OpenAndProbe: return L"Open + Probe Host";
    case CommandType::ApplyCanSettings: return L"Configure CAN";
    case CommandType::ConnectF13Transfer: return L"Discover transfer target";
    case CommandType::RunF13TransferTest: return L"Custom Transfer";
    case CommandType::DiscoverF13Boot: return L"Discover update target";
    case CommandType::EnterF13Bootloader: return L"Enter device Boot";
    case CommandType::ProgramF13Boot: return L"CAN device update";
    case CommandType::VerifyInstalled: return L"Verify installed image";
    case CommandType::CanTxTest: return L"Bridge self test";
    case CommandType::ResetHost: return L"Reset host";
    case CommandType::ProbeI2c: return L"I2C reference discovery";
    case CommandType::UartDiscover: return L"UART device discovery";
    case CommandType::UartEnter: return L"Enter UART device Boot";
    case CommandType::UartProgram: return L"UART device update";
    case CommandType::UartTest: return L"UART Custom Transfer";
    case CommandType::UartVerify: return L"UART read-only VERIFY";
    case CommandType::BridgeInfo: return L"Read host update info";
    case CommandType::BridgeEnter: return L"Enter host Boot";
    case CommandType::BridgeProgram: return L"Host App update";
    case CommandType::BridgeRun: return L"Explicit host RUN confirmation";
    default: return L"Session operation";
    }
}
void WorkerSession::QueueEvent(WorkerEvent event) {
    HWND notify_window;
    UINT notify_message;
    {
        std::lock_guard<std::mutex> lock(event_mutex_);
        if(!events_.Push(std::move(event))) return;
    }
    {
        std::lock_guard<std::mutex> lock(command_mutex_);
        notify_window = notify_window_;
        notify_message = notify_message_;
    }
    if (notify_window == nullptr || notify_message < WM_APP ||
        !PostMessageW(notify_window, notify_message, 0U, 0)) {
        std::lock_guard<std::mutex> lock(event_mutex_);
        events_.RetryWake();
    }
}

} // namespace firmware_upgrade::ui
