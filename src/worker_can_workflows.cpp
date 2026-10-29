#include "worker_session.h"
#include "worker_format.h"
#include "core/firmware_update_profile.h"
#include "core/update_evidence.h"
#include "core/app_confirmation_timing.h"
#include "core/device_version_policy.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>
namespace firmware_upgrade::ui {
using namespace worker_detail;
bool WorkerSession::ProcessApplyCanSettings(const Command& command,
                                           std::wstring* error) {
    if (!state_.host_validated || !state_.serial_open || state_.can_started) {
        *error = L"CAN configuration requires a validated bridge with CAN stopped.";
        return false;
    }
    can_host::Configuration configuration;
    configuration.mode = command.can_settings.mode;
    configuration.nominal_bitrate_preset = command.can_settings.nominal_bitrate_preset;
    configuration.data_bitrate_preset = command.can_settings.data_bitrate_preset;
    if (!can_host::IsConfigurationSupported(configuration, state_.host_capabilities, error)) {
        return false;
    }
    PostLog(L"Applying CAN configuration by UART: nominal " +
            std::wstring(can_host::NominalPresetName(configuration.nominal_bitrate_preset)) + L".");
    if (!PrepareCanPort(command.can_settings, error) ||
        !can_client_.SetConfiguration(configuration, error)) return false;
    state_.can_summary = L"Configured, CAN stopped: " +
        std::wstring(configuration.mode == can_host::Mode::CanFd ? L"CAN FD " : L"Classical CAN ") +
        can_host::NominalPresetName(configuration.nominal_bitrate_preset) +
        (configuration.mode == can_host::Mode::CanFd ? L"/2M" : L"");
    PostLog(state_.can_summary + L". Target bitrate is not changed by this command.");
    PostState();
    return true;
}

bool WorkerSession::ProcessConnectF13Transfer(const Command& command,
                                              std::wstring* error) {
    if (!ConfigureF13Can(
            f13_transfer::kResponseId, f13_transfer::kResponseMask,
            L"0x711 / 0x7FF", command.can_settings, error)) {
        return false;
    }
    PostLog(L"CAN started; discovering a Custom Transfer v1 target.");

    f13_transfer::DeviceInfo device_info;
    if (!transfer_client_.Discover(&device_info, error)) {
        if (!IsCancelRequested()) {
            StopCanBestEffort();
            PostState();
        }
        return false;
    }

    state_.target_validated = true;
    state_.target_profile = TargetProfile::F13Transfer;
    state_.transfer_metadata = device_info;
    state_.target_summary =
        L"Custom Transfer " + NarrowAsciiToWide(device_info.part_number) +
        L"  target " + Hex16(device_info.target_id) +
        L"  protocol " + std::to_wstring(device_info.protocol_major) +
        L"." + std::to_wstring(device_info.protocol_minor);
    PostLog(L"Target validated: " + state_.target_summary);
    PostState();
    return true;
}

bool WorkerSession::ProcessCanTxTest(const Command& command,
                                     std::wstring* error) {
    can_host::Configuration configuration;
    can_host::Filter filter;
    can_host::Frame frame;
    can_host::TransmitResult result;
    std::uint32_t controller_state = 0U;
    std::uint64_t transmit_count = 0U;
    std::uint8_t sequence_counter = 0U;

    if (!state_.serial_open || !state_.host_validated) {
        *error = L"MCU host is not validated.";
        return false;
    }
    if (state_.can_started) {
        *error = L"CAN is already started. Disconnect the target first.";
        return false;
    }
    if (command.can_settings.mode != can_host::Mode::Classical) {
        *error = L"Bridge board self test requires Classical CAN.";
        return false;
    }

    configuration.mode = can_host::Mode::Classical;
    configuration.nominal_bitrate_preset = command.can_settings.nominal_bitrate_preset;
    configuration.data_bitrate_preset = 0xFFU;
    if (!can_host::IsConfigurationSupported(configuration, state_.host_capabilities, error)) {
        return false;
    }
    filter.enabled = true;
    filter.extended = false;
    filter.remote = false;
    filter.id = 0x123U;
    filter.mask = 0x7FFU;

    if (!PrepareCanPort(command.can_settings, error) ||
        !can_client_.SetConfiguration(configuration, error) ||
        !can_client_.SetFilter(0U, filter, error) ||
        !can_client_.Start(&controller_state, error)) {
        return false;
    }

    state_.can_started = true;
    state_.can_tx_test_active = true;
    state_.can_summary = L"Bridge board self test, Classical CAN " +
        std::wstring(can_host::NominalPresetName(configuration.nominal_bitrate_preset)) +
        L", standard ID 0x123, DLC 8, byte 7 counter, period 1000 ms";
    PostLog(state_.can_summary);
    PostLog(L"Bridge board self test started: ID=0x123, DLC=8, "
            L"data=[11 22 33 44 55 66 77 XX], XX=00 initially and "
            L"increments per frame, period=1000 ms, state=" +
            Hex32(controller_state) + L".");
    PostState();

    frame.id = 0x123U;
    frame.dlc = 8U;
    frame.data = {0x11U, 0x22U, 0x33U, 0x44U,
                  0x55U, 0x66U, 0x77U, 0x00U};

    while (!can_tx_test_stop_requested_.load() && !IsCancelRequested()) {
        frame.data[7] = sequence_counter;
        if (!can_client_.Transmit(frame, &result, error)) {
            StopCanBestEffort();
            state_.can_tx_test_active = false;
            PostState();
            return false;
        }
        ++transmit_count;
        PostLog(L"Bridge board self-test frame #" +
                std::to_wstring(transmit_count) + L": ID=0x123, DLC=8, "
                L"counter=" + Hex16(sequence_counter) + L", "
                L"timestamp=" + std::to_wstring(result.completion_timestamp) +
                L", controller_result=" +
                std::to_wstring(result.controller_result) + L".");
        if (result.diagnostic_available) {
            PostLog(L"Bridge TX diagnostic: input_head=[" +
                    Hex16(result.input_head[0]) + L" " +
                    Hex16(result.input_head[1]) + L" " +
                    Hex16(result.input_head[2]) + L" " +
                    Hex16(result.input_head[3]) + L"], input_data=[" +
                    Hex16(result.input_data[0]) + L" " +
                    Hex16(result.input_data[1]) + L" " +
                    Hex16(result.input_data[2]) + L" " +
                    Hex16(result.input_data[3]) + L"], RAM IDL/IDH=" +
                    Hex16(result.ram_id_low) + L"/" +
                    Hex16(result.ram_id_high) + L", DLCREG=" +
                    Hex16(result.ram_dlc_register) + L", FDCTR=" +
                    Hex16(result.ram_fd_control) + L", RAM data=[" +
                    Hex16(result.ram_data[0]) + L" " +
                    Hex16(result.ram_data[1]) + L" " +
                    Hex16(result.ram_data[2]) + L" " +
                    Hex16(result.ram_data[3]) + L"], window=" +
                    Hex16(result.ram_window_before) + L".");
        }
        ++sequence_counter;

        for (unsigned wait_slice = 0U;
             wait_slice < 20U &&
             !can_tx_test_stop_requested_.load() &&
             !IsCancelRequested();
             ++wait_slice) {
            Sleep(50U);
        }
    }

    can_tx_test_stop_requested_ = false;
    state_.can_tx_test_active = false;
    StopCanBestEffort();
    PostState();
    PostLog(L"Bridge board self test stopped after " +
            std::to_wstring(transmit_count) + L" frame(s).");
    return true;
}

bool WorkerSession::ConfigureF13Can(
    std::uint32_t response_id,
    std::uint32_t response_mask,
    const std::wstring& filter_description,
    const CanSettings& settings,
    std::wstring* error) {
    if (!state_.serial_open || !state_.host_validated) {
        *error = L"MCU host is not validated.";
        return false;
    }
    if (state_.can_started) {
        *error = L"CAN is already started. Disconnect the target first.";
        return false;
    }

    can_host::Configuration configuration;
    configuration.mode = settings.mode;
    boot_client_.SetCanFd(settings.mode == can_host::Mode::CanFd);
    transfer_client_.SetCanFd(settings.mode == can_host::Mode::CanFd);
    configuration.nominal_bitrate_preset =
        settings.nominal_bitrate_preset;
    configuration.data_bitrate_preset = settings.data_bitrate_preset;
    if (!can_host::IsConfigurationSupported(configuration, state_.host_capabilities, error)) {
        return false;
    }

    can_host::Filter filter;
    filter.enabled = true;
    filter.extended = settings.protocol_extended_id;
    filter.remote = false;
    filter.id = response_id;
    filter.mask = response_mask;
    std::wstring effective_filter = filter_description;
    if ((settings.filter_selection == FilterSelection::CustomExtended) !=
            settings.protocol_extended_id &&
        settings.filter_selection != FilterSelection::ProfileDefault) {
        *error = L"Custom filter ID format does not match the update "
                 L"protocol metadata.";
        return false;
    }
    if (settings.filter_selection != FilterSelection::ProfileDefault) {
        const std::uint32_t maximum_id =
            settings.protocol_extended_id ? 0x1FFFFFFFU : 0x7FFU;
        if (settings.filter_id > maximum_id ||
            settings.filter_mask == 0U ||
            settings.filter_mask > maximum_id) {
            *error = L"Custom filter ID/mask is invalid.";
            return false;
        }
        if ((response_id & settings.filter_mask) !=
            (settings.filter_id & settings.filter_mask)) {
            *error = L"Custom filter does not accept the selected workflow's "
                     L"response ID.";
            return false;
        }
        filter.id = settings.filter_id;
        filter.mask = settings.filter_mask;
        effective_filter = Hex32(filter.id) + L" / " + Hex32(filter.mask);
    }

    std::uint32_t controller_state = 0U;
    const std::wstring mode_description =
        std::wstring(settings.mode == can_host::Mode::CanFd ? L"CAN FD " : L"Classical CAN ") +
        can_host::NominalPresetName(settings.nominal_bitrate_preset) +
        (settings.mode == can_host::Mode::CanFd ? L"/2M" : L"");
    PostLog(L"Configuring " + mode_description + L".");
    if (!PrepareCanPort(settings, error)) return false;
    if (!can_client_.SetConfiguration(configuration, error)) {
        return false;
    }
    PostLog(L"Applying " +
            std::wstring(settings.protocol_extended_id ?
                             L"extended-data" : L"standard-data") +
            L" RX filter " +
            effective_filter + L".");
    if (!can_client_.SetFilter(0U, filter, error) ||
        !can_client_.Start(&controller_state, error)) {
        return false;
    }
    PostLog(L"CAN controller started, state " +
            Hex32(controller_state) + L".");
    state_.can_started = true;
    state_.target_validated = false;
    state_.f13_bootloader_ready = false;
    state_.target_profile = TargetProfile::None;
    state_.target_summary.clear();
    state_.can_summary = mode_description + L", " +
        std::wstring(settings.protocol_extended_id ?
                         L"extended" : L"standard") +
        L" data RX filter " + effective_filter;
    PostState();
    return true;
}

bool WorkerSession::ProcessRunF13TransferTest(std::wstring* error) {
    if (!state_.target_validated || !state_.can_started) {
        *error = L"Custom Transfer target is not connected.";
        return false;
    }

    const std::array<std::uint8_t, 3> ping_token{{0x12U, 0x34U, 0x56U}};
    const std::array<std::uint8_t, 3> echo_input{{0x00U, 0xA5U, 0xFFU}};
    const std::array<std::uint8_t, 3> pattern_input{{0x5AU, 0xC3U, 0x81U}};
    std::array<std::uint8_t, 3> output{};

    PostProgress(5, L"PING");
    if (!transfer_client_.Ping(ping_token, error)) {
        return false;
    }
    PostLog(L"Custom Transfer PING passed.");

    PostProgress(20, L"ECHO");
    if (!transfer_client_.Echo(echo_input, &output, error)) {
        return false;
    }
    PostLog(L"Custom Transfer ECHO passed.");

    PostProgress(35, L"PATTERN_TEST");
    if (!transfer_client_.PatternTest(pattern_input, &output, error)) {
        return false;
    }
    PostLog(L"Custom Transfer PATTERN_TEST passed.");

    if (transfer_client_.IsCanFd()) {
        if ((state_.transfer_metadata.capabilities & CT_FD64_CAPABILITY) == 0U) {
            *error = L"FD target does not advertise ECHO_FD64 capability bit6; 64-byte verification not run.";
            return false;
        }
        PostProgress(40, L"ECHO_FD64");
        for (unsigned pattern=0U; pattern<3U; ++pattern) {
            std::array<std::uint8_t, CT_FD64_PAYLOAD> fd_data{};
            for (std::size_t index=0U; index<fd_data.size(); ++index)
                fd_data[index] = static_cast<std::uint8_t>(pattern==0U ? index :
                    pattern==1U ? 0xFFU-index : (index%2U ? 0x55U : 0xAAU));
            if (!transfer_client_.EchoFd64(fd_data, error)) return false;
        }
        PostLog(L"Custom Transfer ECHO_FD64 passed: 3 patterns, FD+BRS, DLC15 / 64 bytes each, full59-byte echo and CRC8.");
    }

    for (std::uint8_t index = 0U; index <= 11U; ++index) {
        std::uint16_t value = 0U;
        if (!transfer_client_.GetStat(index, &value, error)) {
            *error += L" Custom Transfer GET_STATS index=" + std::to_wstring(index) + L".";
            return false;
        }
        PostLog(L"Custom Transfer statistic[" + std::to_wstring(index) + L"]=" +
                std::to_wstring(value) + L".");
        PostProgress(40 + static_cast<int>(index + 1U) * 5,
                     L"GET_STATS");
    }

    PostProgress(100, L"Transfer test");
    PostLog(L"Custom Transfer test completed successfully.");
    RefreshHostStatsBestEffort();
    return true;
}

bool WorkerSession::ProcessDiscoverF13Boot(const Command& command,
                                           std::wstring* error) {
    const std::uint32_t response_mask =
        command.can_settings.protocol_extended_id ? 0x1FFFFFFFU : 0x7FFU;
    if (!boot_client_.SetCanIdentifiers(
            command.can_settings.protocol_request_id,
            command.can_settings.protocol_response_id,
            command.can_settings.protocol_extended_id, error)) {
        return false;
    }
    if (!ConfigureF13Can(
            command.can_settings.protocol_response_id, response_mask,
            Hex32(command.can_settings.protocol_response_id) + L" / " +
                Hex32(response_mask),
            command.can_settings, error)) {
        return false;
    }

    PostLog(L"CAN started; reading non-destructive CAN Upgrade metadata.");
    f13_boot::TargetInfo info;
    if (!boot_client_.GetTargetInfo(&info, error)) {
        if (!IsCancelRequested()) {
            StopCanBestEffort();
            PostState();
        }
        return false;
    }
    const std::uint8_t required_mode =
        command.can_settings.mode == can_host::Mode::CanFd ? 0x02U : 0x01U;
    const std::uint32_t nominal_mask =
        1UL << command.can_settings.nominal_bitrate_preset;
    const std::uint32_t data_mask =
        command.can_settings.mode == can_host::Mode::CanFd ?
            1UL << command.can_settings.data_bitrate_preset : 0U;
    if (info.request_id != command.can_settings.protocol_request_id ||
        info.response_id != command.can_settings.protocol_response_id ||
        info.can_id_format !=
            (command.can_settings.protocol_extended_id ? 1U : 0U) ||
        (info.can_mode_mask & required_mode) == 0U ||
        (info.nominal_bitrate_mask & nominal_mask) == 0U ||
        (data_mask != 0U && (info.data_bitrate_mask & data_mask) == 0U)) {
        *error = L"Discovered target CAN metadata does not match the selected "
                 L"mode, bitrate, or protocol identifiers.";
        StopCanBestEffort();
        PostState();
        return false;
    }
    std::wstring diagnostic_error;
    if(!boot_client_.GetDiagnostics(&info.diagnostics,&diagnostic_error)) {
        if(IsCancelRequested()) {*error=L"Target discovery canceled.";return false;}
        info.diagnostics=0U;
        PostLog(L"Optional device diagnostics unavailable; VERIFY stays disabled.");
    }
    state_.target_validated = true;
    state_.target_profile = TargetProfile::CanUpgrade;
    state_.target_metadata = info;
    state_.f13_bootloader_ready =
        info.state == f13_boot::DeviceState::Bootloader;
    state_.target_summary = info.mcu_family + L" " + info.mcu_part + L" " +
        std::wstring(state_.f13_bootloader_ready ? L"bootloader" :
                                                   L"application") +
        L"  target " + Hex16(info.target_id) +
        L"  protocol " + std::to_wstring(info.protocol_version) +
        L"  boot " + VersionText(info.boot_version) + L"/" +
        std::to_wstring(info.boot_build) + L"  app " +
        VersionText(info.app_version) + L"/" +
        (info.app_build == 0xFFFFFFFFU ? L"not installed" :
            std::to_wstring(info.app_build)) +
        L"  installed size " +
        (info.installed_app_size == 0xFFFFFFFFU ? L"not installed" :
            std::to_wstring(info.installed_app_size) + L" bytes") +
        L"  start " + Hex32(info.application_start) +
        L"  capacity " + std::to_wstring(info.application_capacity) +
        L" bytes";
    PostLog(L"Target validated: " + state_.target_summary);
    RefreshHostStatsBestEffort();
    PostState();

    if (state_.f13_bootloader_ready &&
        !ValidateF13BootTarget(error)) {
        StopCanBestEffort();
        PostState();
        return false;
    }
    return true;
}

bool WorkerSession::ProcessEnterF13Bootloader(const Command& command,
                                              std::wstring* error) {
    if (!state_.can_started) {
        const std::uint32_t response_mask =
            command.can_settings.protocol_extended_id ?
                0x1FFFFFFFU : 0x7FFU;
        if (!boot_client_.SetCanIdentifiers(
                command.can_settings.protocol_request_id,
                command.can_settings.protocol_response_id,
                command.can_settings.protocol_extended_id, error)) {
            return false;
        }
        if (!ConfigureF13Can(
                command.can_settings.protocol_response_id, response_mask,
                Hex32(command.can_settings.protocol_response_id) + L" / " +
                    Hex32(response_mask),
                command.can_settings, error)) {
            return false;
        }
    } else if (!state_.target_validated ||
               state_.target_profile != TargetProfile::CanUpgrade ||
               state_.f13_bootloader_ready) {
        *error = L"Discover a running CAN Upgrade application before ENTER.";
        return false;
    }

    PostLog(L"Sending explicit ENTER command to the target application.");
    if (!boot_client_.EnterBootloader(error)) {
        if (!IsCancelRequested()) {
            StopCanBestEffort();
            PostState();
        }
        return false;
    }

    const auto installed_bytes=state_.target_metadata.installed_app_size;
    const evidence::AppConfirmationTiming enter_timing(
        evidence::DeviceRestartQuietMs(installed_bytes,0U,true));
    PostLog(L"ENTER acknowledged; waiting "+std::to_wstring(enter_timing.QuietMs())+
        L" ms without CAN requests for installed App "+
        (installed_bytes==0U || installed_bytes==0xFFFFFFFFU ? L"unknown size" :
            std::to_wstring(installed_bytes)+L" bytes")+L".");
    const auto enter_started=std::chrono::steady_clock::now();
    const auto enter_elapsed=[&]() { return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-enter_started).count()); };
    if (evidence::AwaitAppProbe(enter_timing,enter_elapsed,
        [this]() { return IsCancelRequested(); },[](std::uint32_t ms) { Sleep(ms); })!=
        evidence::AppConfirmationTiming::Action::Probe) {
        *error=IsCancelRequested()?L"Enter bootloader canceled.":L"Enter bootloader wait expired.";
        return false;
    }

    for (unsigned attempt = 1U; attempt <= 3U; ++attempt) {
        if (attempt > 1U) {
            PostLog(L"Bootloader HELLO attempt " +
                    std::to_wstring(attempt) + L" of 3.");
        }
        if (ValidateF13BootTarget(error)) {
            return true;
        }
        if (IsCancelRequested()) {
            return false;
        }
    }

    StopCanBestEffort();
    PostState();
    return false;
}

bool WorkerSession::ValidateF13BootTarget(std::wstring* error) {
    f13_boot::TargetInfo info;
    if (!boot_client_.GetTargetInfo(&info, error)) {
        return false;
    }
    if (info.state != f13_boot::DeviceState::Bootloader) {
        *error = L"F13 target is still running the application.";
        return false;
    }
    if (!boot_client_.SetTargetLayout(
            info.application_start, info.application_capacity,
            info.erase_block_size, error)) {
        return false;
    }

    f13_boot::HelloInfo hello;
    if (!boot_client_.Hello(&hello, error)) {
        return false;
    }
    if (hello.target_id != info.target_id ||
        hello.maximum_update_size_kib !=
            info.application_capacity / 1024U) {
        *error = L"HELLO identity does not match target metadata.";
        return false;
    }

    state_.target_validated = true;
    state_.target_profile = TargetProfile::CanUpgrade;
    state_.target_metadata = info;
    std::wstring diagnostic_error;
    if(!boot_client_.GetDiagnostics(&state_.target_metadata.diagnostics,&diagnostic_error)) {
        if(IsCancelRequested()) {*error=L"Boot diagnostics canceled.";return false;}
        state_.target_metadata.diagnostics=0U;
    }
    state_.f13_bootloader_ready = true;
    state_.target_summary =
        info.mcu_family + L" " + info.mcu_part +
        L" bootloader  target " + Hex16(hello.target_id) +
        L"  protocol " + std::to_wstring(hello.protocol_version) +
        L"  boot " + VersionText(info.boot_version) + L"/" +
        std::to_wstring(info.boot_build) + L"  installed app " +
        VersionText(info.app_version) + L"/" +
        (info.app_build == 0xFFFFFFFFU ? L"not installed" :
            std::to_wstring(info.app_build)) +
        L"  installed size " +
        (info.installed_app_size == 0xFFFFFFFFU ? L"not installed" :
            std::to_wstring(info.installed_app_size) + L" bytes") +
        L"  app start " + Hex32(info.application_start) +
        L"  image format " + std::to_wstring(hello.image_format) +
        L"  capacity " +
        std::to_wstring(hello.maximum_update_size_kib) + L" KiB";
    PostLog(L"Target validated: " + state_.target_summary);
    RefreshHostStatsBestEffort();
    PostState();
    return true;
}

bool WorkerSession::ProcessProgramF13Boot(
    const Command& command,
    std::wstring* error) {
    if (!state_.can_started || !state_.target_validated ||
        state_.target_profile != TargetProfile::CanUpgrade ||
        !state_.f13_bootloader_ready) {
        *error = L"CAN Upgrade bootloader target is not validated.";
        return false;
    }

    f13_boot::TargetInfo live;
    if(!boot_client_.GetTargetInfo(&live,error) || live.state!=f13_boot::DeviceState::Bootloader) {
        if(error->empty()) *error=L"Device is no longer in Boot; update not started.";
        return false;
    }
    state_.target_metadata=live;
    firmware_update::ImageInfo metadata;
    const auto required_burst = can_host::BurstFeatureFor(command.can_settings.mode);
    if ((state_.host_capabilities.feature_flags & required_burst) == 0U) {
        *error = L"CAN Upgrade requires matching bridge burst support (Classical bit12 / FD bit13). Update the bridge App in Misc first.";
        return false;
    }
    if (!firmware_update::ValidateImage(
            firmware_update::Profile::CanUpgrade, command.image,
            &metadata, error) ||
        !ImageMatchesTarget(metadata, state_.target_metadata, error) ||
        !boot_client_.SetTargetLayout(
            state_.target_metadata.application_start,
            state_.target_metadata.application_capacity,
            state_.target_metadata.erase_block_size, error)) {
        return false;
    }

    if(!device_version::Check(metadata,live,command.force_old_version,error)) return false;
    if(command.force_old_version && live.app_version!=command.approved_installed_version) {
        *error=L"Installed version changed after confirmation; force operation canceled before START.";
        return false;
    }
    PostLog(command.force_old_version ?
        L"force upgrade old version firmware: confirmed for this update only; all identity/layout/CRC checks remain active.":
        L"Installed App version policy accepted this normal update.");
    f13_boot::ImageInfo image_info;
    if (!f13_boot::ParseUpdateImage(command.image, &image_info, error)) {
        return false;
    }

    std::wstringstream version;
    version << L"0x" << std::hex << std::uppercase << std::setfill(L'0')
            << std::setw(8) << image_info.version;
    PostLog(L"Firmware image validated: " + metadata.mcu_family + L" " +
            metadata.mcu_part + L", target " +
            Hex16(image_info.target_id) + L", version " + version.str() +
            L", size " + std::to_wstring(image_info.image_size) +
            L" bytes, CRC32 " + Hex32(image_info.calculated_crc32) + L".");
    state_.image_summary = metadata.mcu_family + L" " + metadata.mcu_part +
        L", target " + Hex16(image_info.target_id) +
        L", version " + version.str() + L", size " +
        std::to_wstring(image_info.image_size) + L", CRC32 " +
        Hex32(image_info.calculated_crc32);
    PostState();

    f13_boot::UpdateCallbacks callbacks;
    callbacks.force_old_version=command.force_old_version;
    callbacks.is_canceled = [this]() { return IsCancelRequested(); };
    callbacks.abort=[this](f13_boot::Ack* ack,std::wstring* failure) {
        const bool old=cleanup_in_progress_; cleanup_in_progress_=true;
        const bool result=boot_client_.Abort(ack,failure);
        cleanup_in_progress_=old; return result;
    };
    callbacks.progress = [this](std::uint32_t completed,
                                std::uint32_t total) {
        const int percent = static_cast<int>(completed * 100U / total);
        PostProgress(percent, L"3/5 Transfer");
    };
    callbacks.stage=[this](f13_boot::UpdateStage stage) {
        const wchar_t* names[]={L"1/5 Metadata",L"2/5 START",L"3/5 Transfer",L"4/5 END CRC"};
        const auto name=names[static_cast<unsigned>(stage)];
        if(stage==f13_boot::UpdateStage::Transfer) PostProgress(0,name);
        else PostActivity(name);
        PostLog(std::wstring(L"Update stage: ")+name);
    };
    f13_boot::UpdateReport report;
    PostProgress(0, L"Firmware update");
    PostLog(L"Staging/sealing full256-byte metadata before START; then64-byte BLOCKs/CRC and END. Device rejects policy/layout mismatch before erase.");
    const auto update_started = std::chrono::steady_clock::now();
    const bool programmed=f13_boot::RunUpdate(
            &boot_client_, command.image, callbacks, &report, error);
    update_timings_={{L"metadata",report.metadata_ms},{L"start",report.start_ms},
        {L"transfer",report.transfer_ms},{L"end_crc",report.end_ms}};
    update_details_=L"force_old_version="+std::to_wstring(command.force_old_version)+
        L"; installed_version_before="+evidence::Hex(live.app_version)+
        L"; installed_version_policy="+std::to_wstring(live.version_policy)+
        L"; image_bytes="+std::to_wstring(metadata.image_size)+
        L"; image_crc32="+evidence::Hex(metadata.calculated_crc32)+
        L"; app_version="+evidence::Hex(metadata.version)+L"; app_build="+std::to_wstring(metadata.build)+
        L"; metadata_ms="+std::to_wstring(report.metadata_ms)+L"; start_ms="+std::to_wstring(report.start_ms)+
        L"; transfer_ms="+std::to_wstring(report.transfer_ms)+L"; end_crc_ms="+std::to_wstring(report.end_ms)+
        L"; blocks_acked="+std::to_wstring(report.blocks_acked)+
        L"; abort_attempted="+std::to_wstring(report.abort_attempted)+L"; abort_ack="+std::to_wstring(report.abort_acknowledged);
    PostLog(L"Phase timings: "+update_details_);
    if (!programmed) {
        if (report.abort_attempted) {
            PostLog(report.abort_acknowledged ?
                L"F13 Boot ABORT acknowledged after update failure." :
                L"F13 Boot ABORT was attempted but not acknowledged.");
        }
        const bool old=cleanup_in_progress_; cleanup_in_progress_=true;
        StopCanBestEffort(); cleanup_in_progress_=old;
        PostState();
        return false;
    }

    PostProgress(100, L"4/5 END CRC");
    const auto update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - update_started).count();
    PostLog(L"Update summary: " + std::to_wstring(report.image.image_size) + L" bytes, " +
        std::to_wstring(report.blocks_acked) + L" blocks ACKed, " + std::to_wstring(update_ms) +
        L" ms including metadata/END, " + std::to_wstring(update_ms > 0 ?
        (static_cast<std::uint64_t>(report.image.image_size) * 1000U / update_ms) : 0U) + L" B/s.");
    PostLog(L"END acknowledged with matching CRC32; target is resetting to "
            L"the application.");
    PostActivity(L"5/5 App restart");
    state_.f13_bootloader_ready=false;
    state_.target_validated=false;
    const auto confirm_started=std::chrono::steady_clock::now();
    const bool confirmed=ConfirmDeviceApp(metadata,report.end_ms,error);
    update_timings_.push_back({L"app_confirmation",static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-confirm_started).count())});
    update_details_+=L"; app_confirmation="+std::wstring(confirmed?L"PASS":L"NOT CONFIRMED")+
        L"; confirmation_ms="+std::to_wstring(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-confirm_started).count());
    if(!confirmed) {
        const bool old=cleanup_in_progress_; cleanup_in_progress_=true;
        StopCanBestEffort(); cleanup_in_progress_=old;
    } else {
        const bool stopped=StopCanAfterAppConfirmation();
        update_result_=stopped ? feedback::Result::DeviceAppConfirmed :
            feedback::Result::DeviceAppConfirmedCanActive;
        update_details_+=stopped ? L"; host_can_stop=PASS; Host COM retained" :
            L"; host_can_stop=NOT CONFIRMED; CAN state retained";
        PostProgress(100,L"5/5 App confirmed");
    }
    PostState();
    return confirmed;
}

bool WorkerSession::StopCanAfterAppConfirmation() {
    const bool old=cleanup_in_progress_;
    cleanup_in_progress_=true;
    std::uint32_t controller_state=0U;
    std::wstring stop_error;
    const bool stopped=can_client_.Stop(&controller_state,&stop_error);
    cleanup_in_progress_=old;
    if (!stopped) {
        update_details_+=L"; host_can_stop_error="+stop_error;
        PostLog(L"App update succeeded, but CAN stop was not confirmed: "+stop_error);
        return false;
    }
    state_.can_started=false;
    state_.target_validated=false;
    state_.f13_bootloader_ready=false;
    state_.target_profile=TargetProfile::None;
    state_.target_metadata={};
    state_.target_summary.clear();
    PostLog(L"App confirmed; Host CAN stopped. Host COM remains open. Check the App; Discover Target again for another update.");
    return true;
}

bool WorkerSession::ConfirmDeviceApp(const firmware_update::ImageInfo& image,
                                    std::uint64_t end_crc_ms,std::wstring* error) {
    const auto started=std::chrono::steady_clock::now();
    const auto elapsed=[&]() { return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()); };
    evidence::AppConfirmationTiming timing(evidence::DeviceRestartQuietMs(image.image_size,end_crc_ms));
    std::uint32_t probes=0U;
    const auto record=[&]() { update_details_+=L"; app_reset_quiet_ms="+
        std::to_wstring(timing.QuietMs())+L"; app_health_probes="+std::to_wstring(probes); };
    PostLog(L"App restart wait: "+std::to_wstring(timing.QuietMs())+
        L" ms without CAN requests (image "+std::to_wstring(image.image_size)+
        L" bytes, END CRC "+std::to_wstring(end_crc_ms)+L" ms), included in the "+
        std::to_wstring(evidence::AppConfirmationTiming::budget_ms)+
        L" ms confirmation window; Cancel remains available.");
    PostActivity(L"5/5 App restart");
    while(evidence::AwaitAppProbe(timing,elapsed,[this]() { return IsCancelRequested(); },
        [](std::uint32_t ms) { Sleep(ms); })==evidence::AppConfirmationTiming::Action::Probe) {
        ++probes;
        PostActivity(L"5/5 Check APP_READY");
        std::uint32_t health=0U;
        std::wstring probe_error;
        if(boot_client_.GetAppHealth(&health,&probe_error) && (health&0xFFU)==1U) {
            PostActivity(L"5/5 Verify running App");
            f13_boot::TargetInfo live;
            if(!boot_client_.GetTargetInfo(&live,error)) { record(); return false; }
            for(const auto& row:evidence::Compare(image,live))
                update_details_+=L"\n"+row.field+L": image="+row.image+L", live="+row.live+L", "+row.result;
            if(!evidence::ConfirmApp(image,live,health)) {
                *error=L"END CRC succeeded, but the running App identity / version / build / size or health differs from the loaded image.";
                record();
                return false;
            }
            state_.target_metadata=live; state_.target_validated=true;
            state_.target_profile=TargetProfile::CanUpgrade;
            state_.target_summary=live.mcu_family+L" "+live.mcu_part+L" APP_READY; version "+
                VersionText(live.app_version)+L", build "+std::to_wstring(live.app_build)+
                L", bytes "+std::to_wstring(live.installed_app_size)+L", health "+evidence::Hex(health);
            PostLog(L"App confirmation PASS: "+state_.target_summary+
                L". Health covers local clock/CAN/timer checks, not application-level functional acceptance.");
            record(); *error=L""; return true;
        }
        timing.ProbeFinished(elapsed());
    }
    record();
    *error=IsCancelRequested()?L"App confirmation canceled; END had already succeeded. No automatic rewrite.":
        L"END CRC succeeded, but APP_READY was not confirmed within the bounded reconnect window. No automatic rewrite.";
    return false;
}

}

