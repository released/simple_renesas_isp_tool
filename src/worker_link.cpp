#include "worker_session.h"
#include "core/host_link_baud.h"

namespace firmware_upgrade::ui {
bool WorkerSession::OpenHostLink(const std::wstring& port, std::uint32_t desired,
                                host_link::HostInfo* host, host_link::Capabilities* caps,
                                std::wstring* error) {
    if (desired!=115200U && desired!=460800U && desired!=921600U) {
        *error=L"Unsupported PC-to-bridge Host Link baud."; return false;
    }
    // Only the explicitly selected COM. Recover a confirmed high-speed session
    // left behind by cable loss / process exit; never enumerate-and-probe ports.
    for (const auto rate:{115200U,460800U,921600U,115200U}) {
        if (IsCancelRequested()) { *error=L"Host Link open canceled."; return false; }
        PostLog(L"Probing selected "+port+L" at Host Link "+std::to_wstring(rate)+L" baud.");
        if (!serial_port_.Open(port,rate,error)) return false;
        host_link_client_.BeginSession();
        if (!host_link_client_.Probe(0U,host,caps,error)) { serial_port_.Close(); continue; }
        if (!(caps->feature_flags&HL_BAUD_FEATURE)) {
            if (desired!=rate) {
                bu_info_t boot={};
                if (!(caps->feature_flags&BU_FEATURE) ||
                    !bridge_update::ReadInfo(host_link_client_,&boot,error) ||
                    boot.mode!=BU_MODE_BOOT || boot.uart_baud!=rate) {
                    *error=L"Bridge does not advertise Host Link baud switching. Use its fixed COM rate.";
                    serial_port_.Close(); return false;
                }
                PostLog(L"Bridge BOOT uses its fixed self-update UART rate "+std::to_wstring(rate)+
                        L"; high-speed Host Link selection applies to APP only.");
            }
            state_.baudrate=rate;
        } else {
            host_link::LinkBaudInfo info;
            const auto board=host->board_id;
            if (rate!=desired) {
                PostLog(L"Quiescing verified bridge CAN/I2C before Host Link speed change; no target flash commands.");
                // Recover stale reservations after an abnormal PC exit. An
                // initial I2C ABORT may be rejected while CAN is running.
                std::wstring ignored;
                if (caps->feature_flags&bridge::kFeatureI2c) i2c_client_.Abort(&ignored);
                std::uint32_t stopped=0;
                if (caps->maximum_can_data_bytes && !can_client_.Stop(&stopped,error)) {
                    serial_port_.Close(); return false;
                }
                if ((caps->feature_flags&bridge::kFeatureI2c) && !i2c_client_.Abort(error)) {
                    serial_port_.Close(); return false;
                }
            }
            if (!host_link::SwitchLinkBaud(host_link_client_,desired,
                [this](std::uint32_t n,std::wstring* e){return serial_port_.ChangeBaud(n,e);},&info,error)) {
                PostLog(L"Host Link switch failed; waiting for bounded bootstrap recovery.");
                const auto failure=*error;
                Sleep(HL_BAUD_CONFIRM_MS+200U);
                // A lost CONFIRM ACK can leave a committed high speed. Probe
                // both rates and explicitly request bootstrap where possible.
                const bool old=cleanup_in_progress_; cleanup_in_progress_=true;
                for (const auto recovery:{115200U,desired,rate}) {
                    host_link::HostInfo recovered; host_link::Capabilities recovered_caps;
                    std::wstring ignored;
                    if (!serial_port_.Open(port,recovery,&ignored)) break;
                    host_link_client_.BeginSession();
                    if (host_link_client_.Probe(board,&recovered,&recovered_caps,&ignored)) {
                        if (recovered_caps.feature_flags&HL_BAUD_FEATURE)
                            host_link::SwitchLinkBaud(host_link_client_,115200U,
                                [this](std::uint32_t n,std::wstring* e){return serial_port_.ChangeBaud(n,e);},&info,&ignored);
                        break;
                    }
                }
                cleanup_in_progress_=old; serial_port_.Close(); *error=failure; return false;
            }
            if (!host_link_client_.Probe(board,host,caps,error)) { serial_port_.Close(); return false; }
            state_.baudrate=info.current;
            PostLog(L"Host Link nominal "+std::to_wstring(info.current)+L", MCU calculated "+
                std::to_wstring(info.actual)+L" baud. Device update settings are independent.");
        }
        state_.requested_baudrate=desired;
        return true;
    }
    serial_port_.Close(); return false;
}
void WorkerSession::RestoreLinkBaudBestEffort() {
    if (!state_.host_validated || !serial_port_.IsOpen() || state_.baudrate==115200U ||
        !(state_.host_capabilities.feature_flags&HL_BAUD_FEATURE)) return;
    const bool old=cleanup_in_progress_; cleanup_in_progress_=true;
    host_link::LinkBaudInfo info; std::wstring error;
    if (host_link::SwitchLinkBaud(host_link_client_,115200U,
        [this](std::uint32_t n,std::wstring* e){return serial_port_.ChangeBaud(n,e);},&info,&error)) {
        state_.baudrate=115200U; PostLog(L"Host Link restored to115200 before closing COM.");
    } else PostLog(L"Host Link bootstrap restore not confirmed: "+error+
                   L" Reopen probes only this selected COM at the three supported rates.");
    cleanup_in_progress_=old;
}
}
