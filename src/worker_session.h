#pragma once

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/can_host_client.h"
#include "core/bridge_port_catalog.h"
#include "core/i2c_host_client.h"
#include "core/g16_i2c_reference.h"
#include "core/f13_boot_profile.h"
#include "core/f13_boot_update.h"
#include "core/f13_transfer_profile.h"
#include "core/host_link_client.h"
#include "core/serial_port.h"
#include "core/bridge_update_profile.h"

namespace firmware_upgrade::ui {

enum class WorkerEventKind {
    Log,
    State,
    Progress,
    OperationComplete,
    UpdateReport,
};

enum class TargetProfile {
    None,
    F13Transfer,
    CanUpgrade,
};

enum class FilterSelection {
    ProfileDefault,
    CustomStandard,
    CustomExtended,
};

struct CanSettings {
    std::uint8_t port = 0U;
    can_host::Mode mode = can_host::Mode::Classical;
    std::uint8_t nominal_bitrate_preset = 2U;
    std::uint8_t data_bitrate_preset = 0xFFU;
    FilterSelection filter_selection = FilterSelection::ProfileDefault;
    std::uint32_t filter_id = 0U;
    std::uint32_t filter_mask = 0U;
    std::uint32_t protocol_request_id = f13_boot::kRequestId;
    std::uint32_t protocol_response_id = f13_boot::kResponseId;
    bool protocol_extended_id = false;
};

struct SessionState {
    bool serial_open = false;
    bool host_validated = false;
    bool can_started = false;
    bool target_validated = false;
    bool f13_bootloader_ready = false;
    bool can_tx_test_active = false;
    bool busy = false;
    TargetProfile target_profile = TargetProfile::None;
    f13_boot::TargetInfo target_metadata;
    std::wstring port_name;
    std::uint32_t baudrate = 0U;
    std::uint32_t requested_baudrate = 115200U;
    std::wstring host_summary;
    host_link::HostInfo host_info;
    f13_transfer::DeviceInfo transfer_metadata;
    host_link::Capabilities host_capabilities;
    bool bridge_info_valid = false;
    bu_info_t bridge_info = {};
    std::vector<bridge::Port> can_ports;
    std::vector<bridge::Port> i2c_ports;
    bool i2c_probe_validated = false;
    i2c_host::Configuration i2c_configuration;
    std::uint8_t i2c_address = 0U;
    g16_i2c::DeviceInfo i2c_metadata;
    std::wstring i2c_summary;
    std::wstring target_summary;
    std::wstring can_summary;
    std::wstring image_summary;
    std::uint64_t host_requests = 0U;
    std::uint64_t host_retries = 0U;
    std::uint64_t host_timeouts = 0U;
    std::uint64_t unmatched_frames = 0U;
    std::uint64_t received_events = 0U;
    std::uint64_t dropped_events = 0U;
    std::uint32_t arbitration_lost_entries = 0U;
    bool f24_stats_valid = false;
    host_link::HostStats f24_stats;
};

struct WorkerEvent {
    WorkerEventKind kind = WorkerEventKind::Log;
    SessionState state;
    int progress = 0;
    bool success = false;
    std::wstring text;
};

class WorkerSession {
public:
    WorkerSession();
    ~WorkerSession();

    WorkerSession(const WorkerSession&) = delete;
    WorkerSession& operator=(const WorkerSession&) = delete;

    bool Start(HWND notify_window, UINT notify_message);
    void StopAndJoin();

    bool OpenAndProbe(const std::wstring& port_name, std::uint32_t baudrate);
    bool ApplyCanSettings(const CanSettings& settings);
    bool ConnectF13Transfer(const CanSettings& settings);
    bool RunF13TransferTest();
    bool DiscoverF13Boot(const CanSettings& settings);
    bool EnterF13Bootloader(const CanSettings& settings);
    bool ProgramF13Boot(const std::vector<std::uint8_t>& image, bool force_old_version=false,
                        std::uint32_t approved_installed_version=0U);
    bool StartCanTxTest(const CanSettings& settings);
    bool StopCanTxTest();
    bool ResetHost();
    bool BridgeAction(unsigned action, const std::vector<std::uint8_t>& image = {});
    bool ProbeI2c(const i2c_host::Configuration& configuration,
                  std::uint8_t address);
    bool RequestDisconnect();
    bool RequestCancel();
    bool VerifyInstalled();
    void SetDetailedTrace(bool enabled);

    std::vector<WorkerEvent> DrainEvents();

private:
    enum class CommandType {
        OpenAndProbe,
        ApplyCanSettings,
        ConnectF13Transfer,
        RunF13TransferTest,
        DiscoverF13Boot,
        EnterF13Bootloader,
        ProgramF13Boot,
        VerifyInstalled,
        CanTxTest,
        ResetHost,
        ProbeI2c,
        BridgeInfo,
        BridgeEnter,
        BridgeProgram,
        BridgeRun,
        Disconnect,
        Shutdown,
    };

    struct Command {
        CommandType type = CommandType::Shutdown;
        std::wstring port_name;
        std::uint32_t baudrate = 0U;
        CanSettings can_settings;
        std::vector<std::uint8_t> image;
        i2c_host::Configuration i2c_configuration;
        std::uint8_t i2c_address = 0x09U;
        bool force_old_version = false;
        std::uint32_t approved_installed_version = 0U;
    };

    bool Submit(Command command);
    void ThreadMain();
    bool ProcessOpenAndProbe(const Command& command, std::wstring* error);
    bool OpenHostLink(const std::wstring& port, std::uint32_t desired,
                      host_link::HostInfo* host, host_link::Capabilities* caps, std::wstring* error);
    void RestoreLinkBaudBestEffort();
    bool ProcessApplyCanSettings(const Command& command, std::wstring* error);
    bool ProcessConnectF13Transfer(const Command& command,
                                   std::wstring* error);
    bool ProcessRunF13TransferTest(std::wstring* error);
    bool ProcessDiscoverF13Boot(const Command& command,
                                std::wstring* error);
    bool ProcessEnterF13Bootloader(const Command& command,
                                   std::wstring* error);
    bool ProcessProgramF13Boot(const Command& command, std::wstring* error);
    bool ConfirmDeviceApp(const firmware_update::ImageInfo& image, std::wstring* error);
    bool ProcessCanTxTest(const Command& command, std::wstring* error);
    bool ProcessResetHost(std::wstring* error);
    bool ProcessProbeI2c(const Command& command, std::wstring* error);
    bool ProcessBridgeAction(const Command& command, std::wstring* error);
    bool RefreshBridgeInfo(std::wstring* error);
    bool RefreshBridgePorts(std::wstring* error);
    bool PrepareCanPort(const CanSettings& settings, std::wstring* error);
    void AbortI2cBestEffort();
    bool ConfigureF13Can(std::uint32_t response_id,
                         std::uint32_t response_mask,
                         const std::wstring& filter_description,
                         const CanSettings& settings,
                         std::wstring* error);
    bool ValidateF13BootTarget(std::wstring* error);
    void ProcessDisconnect();
    void StopCanBestEffort();
    void RefreshHostStatsBestEffort();
    void PollHostEvents();
    void HandleHostLinkEvent(const host_link::Frame& event);
    void ResetSessionState();
    bool IsCancelRequested() const;
    void PostHostLinkTrace(const host_link::TraceRecord& record);
    void PostCanTrace(can_host::TraceDirection direction,
                      const can_host::Frame& frame);

    void PostLog(const std::wstring& text);
    void PostState();
    void PostProgress(int progress, const std::wstring& text);
    void PostCompletion(bool success, const std::wstring& text);
    void QueueEvent(WorkerEvent event);

    HWND notify_window_ = nullptr;
    UINT notify_message_ = 0U;
    std::thread worker_;
    std::mutex command_mutex_;
    std::condition_variable command_ready_;
    std::deque<Command> commands_;
    bool command_pending_or_busy_ = false;
    bool started_ = false;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> can_tx_test_stop_requested_{false};
    std::atomic<bool> detailed_trace_enabled_{false};
    bool cleanup_in_progress_ = false;

    std::mutex event_mutex_;
    std::deque<WorkerEvent> events_;
    std::uint64_t dropped_ui_events_ = 0U;

    SessionState state_;
    std::wstring update_details_;
    std::vector<std::pair<std::wstring,std::uint64_t>> update_timings_;
    host_link::SerialPort serial_port_;
    host_link::Client host_link_client_;
    can_host::Client can_client_;
    i2c_host::Client i2c_client_;
    f13_boot::Client boot_client_;
    f13_transfer::Client transfer_client_;
};

} // namespace firmware_upgrade::ui
