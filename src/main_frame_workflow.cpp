#include "main_frame.h"

#include "core/firmware_update_profile.h"
#include "core/device_version_policy.h"

#include <afxdlgs.h>

#include <cwchar>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::wstring VersionText(std::uint32_t version) {
    if (version == 0xFFFFFFFFU) return L"not installed";
    return std::to_wstring((version >> 16U) & 0xFFU) + L"." +
        std::to_wstring((version >> 8U) & 0xFFU) + L"." +
        std::to_wstring(version & 0xFFU);
}

bool ParseUnsignedText(const std::wstring& text,
                       std::uint32_t* value) {
    if (value == nullptr || text.empty()) {
        return false;
    }
    try {
        std::size_t consumed = 0U;
        const unsigned long long parsed = std::stoull(text, &consumed, 0);
        if (consumed != text.size() || parsed > 0xFFFFFFFFULL) {
            return false;
        }
        *value = static_cast<std::uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::wstring Hex32(std::uint32_t value) {
    std::wstringstream stream;
    stream << L"0x" << std::hex << std::uppercase << std::setfill(L'0')
           << std::setw(8) << value;
    return stream.str();
}

bool ImageMatchesTarget(
    const firmware_upgrade::firmware_update::ImageInfo& image,
    const firmware_upgrade::f13_boot::TargetInfo& target) {
    return image.policy.role==IP_ROLE_DEVICE_APP && (image.policy.transports&IP_CAN) &&
           target.policy.role==IP_ROLE_DEVICE_APP && (target.policy.transports&IP_CAN) &&
           image.policy.irq_abi==target.policy.irq_abi && image.policy.minimum_boot_version<=target.boot_version &&
           image.target_id == target.target_id &&
           image.protocol_id == target.protocol_id &&
           image.protocol_version == target.protocol_version &&
           image.format == target.image_format &&
           image.checksum_type == target.checksum_type &&
           image.application_start == target.application_start &&
           image.application_capacity == target.application_capacity &&
           image.code_flash_size == target.code_flash_size &&
           image.boot_reserved_size == target.boot_reserved_size &&
           image.erase_block_size == target.erase_block_size &&
           image.minimum_image_size == target.minimum_image_size &&
           image.can_mode_mask == target.can_mode_mask &&
           image.can_id_format == target.can_id_format &&
           image.nominal_bitrate_mask == target.nominal_bitrate_mask &&
           image.data_bitrate_mask == target.data_bitrate_mask &&
           image.request_id == target.request_id &&
           image.response_id == target.response_id &&
           image.mcu_part == target.mcu_part &&
           image.mcu_family == target.mcu_family;
}

} // namespace

std::wstring CMainFrame::SelectedCanBitrate() const {
    CString text;
    can_bitrate_combo_.GetWindowTextW(text);
    return text.GetString();
}

bool CMainFrame::SelectedCanSettings(
    firmware_upgrade::ui::CanSettings* settings,
    std::wstring* error) const {
    if (settings == nullptr) {
        if (error != nullptr) {
            *error = L"CAN settings output is null.";
        }
        return false;
    }

    CString text;
    firmware_upgrade::ui::CanSettings parsed;
    parsed.port = preferred_can_port_;
    const int selected_port = bus_port_combo_.GetCurSel();
    if (!IsI2cTab() && selected_port >= 0)
        parsed.port = static_cast<std::uint8_t>(bus_port_combo_.GetItemData(selected_port));
    can_mode_combo_.GetWindowTextW(text);
    parsed.mode = std::wstring(text.GetString()) == L"CAN FD" ?
        firmware_upgrade::can_host::Mode::CanFd :
        firmware_upgrade::can_host::Mode::Classical;
    can_bitrate_combo_.GetWindowTextW(text);
    const std::wstring nominal = text.GetString();
    if (!firmware_upgrade::can_host::NominalPresetFromText(nominal, &parsed.nominal_bitrate_preset)) {
        if (error != nullptr) *error = L"Select a supported nominal CAN bitrate.";
        return false;
    }
    can_data_combo_.GetWindowTextW(text);
    const std::wstring data = text.GetString();
    parsed.data_bitrate_preset = parsed.mode ==
        firmware_upgrade::can_host::Mode::CanFd ?
            (data == L"1M" ? 0U : data == L"4M" ? 2U :
             data == L"5M" ? 3U : data == L"8M" ? 4U : 1U) : 0xFFU;

    if (SelectedProfile() == L"CAN Upgrade" &&
        binary_inspection_.metadata_valid) {
        parsed.protocol_request_id =
            binary_inspection_.metadata.request_id;
        parsed.protocol_response_id =
            binary_inspection_.metadata.response_id;
        parsed.protocol_extended_id =
            binary_inspection_.metadata.can_id_format == 1U;
    }

    can_filter_combo_.GetWindowTextW(text);
    const std::wstring filter = text.GetString();
    if (filter == L"Custom standard") {
        parsed.filter_selection =
            firmware_upgrade::ui::FilterSelection::CustomStandard;
    } else if (filter == L"Custom extended") {
        parsed.filter_selection =
            firmware_upgrade::ui::FilterSelection::CustomExtended;
    } else {
        parsed.filter_selection =
            firmware_upgrade::ui::FilterSelection::ProfileDefault;
    }

    if (parsed.filter_selection !=
        firmware_upgrade::ui::FilterSelection::ProfileDefault) {
        const std::uint32_t maximum_id =
            parsed.filter_selection ==
                    firmware_upgrade::ui::FilterSelection::CustomExtended ?
                0x1FFFFFFFU : 0x7FFU;
        can_filter_id_edit_.GetWindowTextW(text);
        if (!ParseUnsignedText(text.GetString(), &parsed.filter_id) ||
            parsed.filter_id > maximum_id) {
            if (error != nullptr) {
                *error =
                    L"RX filter ID must be a valid decimal or 0x hex value.";
            }
            return false;
        }
        can_filter_mask_edit_.GetWindowTextW(text);
        if (!ParseUnsignedText(text.GetString(), &parsed.filter_mask) ||
            parsed.filter_mask == 0U || parsed.filter_mask > maximum_id) {
            if (error != nullptr) {
                *error = L"RX filter mask must be a non-zero decimal or 0x "
                         L"hex value.";
            }
            return false;
        }
    }

    *settings = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

std::wstring CMainFrame::SelectedProfile() const {
    CString text;
    profile_combo_.GetWindowTextW(text);
    return text.GetString();
}

bool CMainFrame::SelectedApplicationStart(
    std::uint32_t* application_start,
    std::wstring* error) const {
    if (application_start == nullptr) {
        if (error != nullptr) {
            *error = L"Application start output is null.";
        }
        return false;
    }
    CString raw_text;
    app_start_edit_.GetWindowTextW(raw_text);
    raw_text.Trim();
    const std::wstring text = raw_text.GetString();
    /*
     * Auto is metadata selection, not address relocation.  Before discovery,
     * the image manifest is the source.  After discovery, the programming
     * prerequisite checks require that the device reports the same address.
     * A numeric value is only an additional user assertion and must still
     * match the image and device metadata before START is enabled.
     */
    if (text.empty() || _wcsicmp(text.c_str(), L"Auto") == 0) {
        if (binary_inspection_.metadata_valid) {
            *application_start =
                binary_inspection_.metadata.application_start;
        } else if (session_state_.target_validated &&
                   session_state_.target_profile ==
                       firmware_upgrade::ui::TargetProfile::CanUpgrade &&
                   session_state_.target_metadata.application_start != 0U) {
            *application_start =
                session_state_.target_metadata.application_start;
        } else {
            if (error != nullptr) {
                *error = L"App Start Auto requires valid image metadata or a "
                         L"discovered target.";
            }
            return false;
        }
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    std::uint32_t parsed = 0U;
    if (!ParseUnsignedText(text, &parsed) || parsed == 0U ||
        (parsed & 3U) != 0U) {
        if (error != nullptr) {
            *error = L"App Start Override must be Auto or a non-zero, "
                     L"4-byte-aligned 32-bit address.";
        }
        return false;
    }
    *application_start = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CMainFrame::QueueOpenSelectedCom() {
    const std::wstring port = SelectedComPort();
    if (port.empty()) {
        AppendLog(L"Open blocked: no COM port is selected.");
        return false;
    }
    pending_com_instance_.clear(); last_probe_port_=port;
    for(const auto& p:com_ports_) if(p.port_name==port) pending_com_instance_=p.instance_id;
    if (!worker_session_.OpenAndProbe(port, SelectedBaudrate())) {
        AppendLog(L"Open blocked: another worker operation is active.");
        return false;
    }

    worker_command_pending_ = true;
    SetProgress(0,L"Progress");
    last_update_result_=firmware_upgrade::feedback::Result::None;
    CString display;
    com_combo_.GetWindowTextW(display);
    AppendLog(L"Queued host open/probe for " + std::wstring(display.GetString()) + L".");
    SaveUiSettings();
    UpdateUi();
    return true;
}

void CMainFrame::QueueDisconnect() {
    if (!worker_session_.RequestDisconnect()) {
        AppendLog(L"Disconnect request was not accepted.");
        return;
    }
    worker_command_pending_ = true;
    AppendLog(L"Disconnect/cancel requested.");
    UpdateUi();
}

bool CMainFrame::IsComOpen() const {
    return session_state_.serial_open;
}

void CMainFrame::RefreshNominalChoices() {
    if (can_bitrate_combo_.GetSafeHwnd() == nullptr) return;
    const std::uint32_t mask = session_state_.host_validated ?
        (session_state_.host_capabilities.nominal_bitrate_mask &
         firmware_upgrade::can_host::kSupportedNominalMask) :
        firmware_upgrade::can_host::kSupportedNominalMask;
    if (mask == displayed_nominal_mask_) return;
    const auto previous = SelectedCanBitrate();
    can_bitrate_combo_.ResetContent();
    for (const auto preset : firmware_upgrade::can_host::kNominalPresets) {
        if ((mask & (1UL << preset)) != 0U)
            can_bitrate_combo_.AddString(firmware_upgrade::can_host::NominalPresetName(preset));
    }
    int selection = can_bitrate_combo_.FindStringExact(-1, previous.c_str());
    if (selection < 0) selection = can_bitrate_combo_.FindStringExact(-1, L"500K");
    if (selection < 0 && can_bitrate_combo_.GetCount() > 0) selection = 0;
    can_bitrate_combo_.SetCurSel(selection);
    displayed_nominal_mask_ = mask;
}

void CMainFrame::QueueCanConfiguration() {
    if (IsI2cTab() || IsMiscTab() || IsUartTab()) return;
    if (!session_state_.host_validated || session_state_.can_started ||
        worker_command_pending_ || session_state_.busy) return;
    firmware_upgrade::ui::CanSettings settings;
    std::wstring error;
    if (!SelectedCanSettings(&settings, &error)) {
        AppendLog(L"CAN configuration blocked: " + error);
        return;
    }
    firmware_upgrade::can_host::Configuration configuration;
    configuration.mode = settings.mode;
    configuration.nominal_bitrate_preset = settings.nominal_bitrate_preset;
    configuration.data_bitrate_preset = settings.data_bitrate_preset;
    if (!firmware_upgrade::can_host::IsConfigurationSupported(
            configuration, session_state_.host_capabilities, &error)) {
        AppendLog(L"CAN configuration blocked: " + error);
        return;
    }
    if (worker_session_.ApplyCanSettings(settings)) {
        worker_command_pending_ = true;
        AppendLog(L"Queued CAN configuration by UART; CAN remains stopped.");
    }
}

void CMainFrame::UpdateUi() {
    if (detailed_trace_check_.GetSafeHwnd() == nullptr ||
        operation_status_card_.GetSafeHwnd() == nullptr) {
        return;
    }
    RefreshNominalChoices();

    const bool busy = worker_command_pending_ || session_state_.busy;
    firmware_prerequisites_label_.SetEmphasized(false);
    const bool port_open = session_state_.serial_open;
    const bool host_ready = session_state_.host_validated;
    const bool can_started = session_state_.can_started;
    const bool can_tx_test_active = session_state_.can_tx_test_active;
    const bool target_ready = session_state_.target_validated;
    const bool boot_profile = SelectedProfile() == L"CAN Upgrade";
    const bool transfer_profile = SelectedProfile() == L"Custom Transfer";
    CString mode_text;
    can_mode_combo_.GetWindowTextW(mode_text);
    const bool classical_mode =
        std::wstring(mode_text.GetString()) == L"Classical CAN";
    CString filter_text;
    can_filter_combo_.GetWindowTextW(filter_text);
    const bool custom_filter =
        std::wstring(filter_text.GetString()) != L"Metadata/default";
    const bool update_target_ready =
        target_ready && session_state_.target_profile ==
                            firmware_upgrade::ui::TargetProfile::CanUpgrade;
    std::uint32_t selected_application_start = 0U;
    std::wstring application_start_error;
    const bool application_start_valid =
        binary_inspection_.metadata_valid &&
        SelectedApplicationStart(
            &selected_application_start, &application_start_error);
    const bool selected_mode_supported =
        !binary_inspection_.metadata_valid ||
        (classical_mode ?
             (binary_inspection_.metadata.can_mode_mask & 0x01U) != 0U :
             (binary_inspection_.metadata.can_mode_mask & 0x02U) != 0U);
    firmware_upgrade::ui::CanSettings selected_can_settings;
    std::wstring selected_can_error;
    const bool can_settings_valid =
        SelectedCanSettings(&selected_can_settings, &selected_can_error);
    const bool selected_bitrate_supported =
        !binary_inspection_.metadata_valid ||
        (can_settings_valid &&
         (binary_inspection_.metadata.nominal_bitrate_mask &
          (1UL << selected_can_settings.nominal_bitrate_preset)) != 0U &&
         (classical_mode ||
          (binary_inspection_.metadata.data_bitrate_mask &
           (1UL << selected_can_settings.data_bitrate_preset)) != 0U));
    const bool image_matches_target =
        binary_inspection_.metadata_valid && update_target_ready &&
        ImageMatchesTarget(binary_inspection_.metadata,
                           session_state_.target_metadata);

    connect_target_btn_.SetWindowTextW(
        boot_profile ? L"Discover Target" : L"Discover Test Target");
    enter_bootloader_btn_.SetWindowTextW(L"Enter Boot");
    transfer_test_btn_.SetWindowTextW(boot_profile?L"Verify Installed":L"Transfer Test");
    start_firmware_update_btn_.SetWindowTextW(L"Start Device Update");
    open_com_btn_.EnableWindow(!busy && !port_open);
    close_com_btn_.EnableWindow(port_open || busy);
    reset_host_btn_.EnableWindow(!busy && host_ready);
    refresh_com_btn_.EnableWindow(!busy && !port_open);
    com_combo_.EnableWindow(!busy && !port_open);
    baud_combo_.EnableWindow(!busy && !port_open);
    cancel_btn_.EnableWindow(busy);
    preflight_btn_.EnableWindow(!busy);
    bin_details_btn_.EnableWindow(!busy && !binary_data_.empty());
    open_report_btn_.EnableWindow(!busy && !latest_report_path_.empty());
    error_details_btn_.EnableWindow(!busy && !last_operation_error_.empty());
    compare_metadata_btn_.EnableWindow(!busy && !binary_data_.empty() &&
        (IsMiscTab()?session_state_.bridge_info_valid:!IsI2cTab() && target_ready));
    force_old_firmware_btn_.ShowWindow(!IsMiscTab() && !IsI2cTab() ? SW_SHOW : SW_HIDE);
    force_old_firmware_btn_.EnableWindow(FALSE);
    if(IsIspTab()) {UpdateUartUi();return;}
    if (IsMiscTab()) {
        load_binary_btn_.EnableWindow(!busy);
        start_log_btn_.EnableWindow(!log_recording_);
        stop_log_btn_.EnableWindow(log_recording_);
        export_summary_btn_.EnableWindow(!busy);
        UpdateBridgeImageFields(); UpdateMiscUi(); UpdateCompletionFeedback(); return;
    }

    can_mode_combo_.EnableWindow(!busy && host_ready && !can_started);
    can_bitrate_combo_.EnableWindow(!busy && host_ready && !can_started);
    can_data_combo_.EnableWindow(
        !busy && host_ready && !can_started && !classical_mode);
    can_tx_test_btn_.SetWindowTextW(
        can_tx_test_active ? L"Stop Bridge Self Test" :
                             L"Start Bridge Self Test");
    can_tx_test_btn_.EnableWindow(
        can_tx_test_active || (!busy && host_ready && !can_started &&
                               classical_mode));
    can_filter_combo_.EnableWindow(!busy && host_ready && !can_started);
    can_filter_id_edit_.EnableWindow(
        !busy && host_ready && !can_started && custom_filter);
    can_filter_mask_edit_.EnableWindow(
        !busy && host_ready && !can_started && custom_filter);
    profile_combo_.EnableWindow(!busy && host_ready && !can_started);
    app_start_edit_.EnableWindow(!busy && boot_profile);
    connect_target_btn_.EnableWindow(
        !busy && host_ready && !can_started && !target_ready &&
        (transfer_profile ? can_settings_valid :
                            selected_mode_supported &&
                                selected_bitrate_supported));
    enter_bootloader_btn_.EnableWindow(
        !busy && host_ready && boot_profile && selected_mode_supported &&
        selected_bitrate_supported &&
        can_started && update_target_ready && !session_state_.f13_bootloader_ready);
    transfer_test_btn_.EnableWindow(
        !busy && target_ready && ((boot_profile && session_state_.f13_bootloader_ready &&
            (session_state_.target_metadata.diagnostics&2U)) ||
        (transfer_profile && session_state_.target_profile == firmware_upgrade::ui::TargetProfile::F13Transfer)));

    const auto image_blocker = firmware_upgrade::feedback::CanImageBlocker(
        selected_mode_supported, selected_bitrate_supported);
    std::vector<std::wstring> missing;
    if (boot_profile) {
        if (busy) {
            missing.push_back(L"wait for the active operation");
        }
        if (!host_ready) {
            missing.push_back(L"Open + Probe Host");
        }
        const auto required_burst = firmware_upgrade::can_host::BurstFeatureFor(selected_can_settings.mode);
        if (host_ready && (session_state_.host_capabilities.feature_flags & required_burst) == 0U) {
            missing.push_back(L"update bridge App: matching CAN burst capability required (Classical bit12 / FD bit13)");
        }
        if (!selected_mode_supported) {
            missing.push_back(
                L"selected CAN mode is not supported by the image");
        }
        if (!selected_bitrate_supported) {
            missing.push_back(
                L"selected CAN bitrate is not supported by the image");
        }
        if (!update_target_ready && selected_mode_supported && selected_bitrate_supported) {
            missing.push_back(L"Discover Target with CAN Upgrade");
        } else if (update_target_ready && !session_state_.f13_bootloader_ready) {
            missing.push_back(L"App is running: select Enter Boot");
        }
        if (binary_data_.empty()) {
            missing.push_back(L"Load Update Image _update.bin");
        } else if (!binary_image_valid_) {
            missing.push_back(L"load a valid CAN Upgrade image: " +
                              binary_image_validation_error_);
        }
        if (binary_inspection_.metadata_valid && !application_start_valid) {
            missing.push_back(application_start_error);
        } else if (binary_inspection_.metadata_valid &&
                   selected_application_start !=
                       binary_inspection_.metadata.application_start) {
            missing.push_back(
                L"App Start Override does not match image metadata");
        }
        if (binary_inspection_.metadata_valid && update_target_ready &&
            !image_matches_target) {
            missing.push_back(L"image metadata does not match the target");
        }

        const bool version_blocked=image_matches_target && binary_image_valid_ &&
            firmware_upgrade::device_version::IsBlocked(binary_inspection_.metadata,session_state_.target_metadata);
        force_old_firmware_btn_.EnableWindow(missing.empty() && version_blocked);
        if(version_blocked) missing.push_back(L"older version blocked; use force upgrade old version firmware for this update only");
        start_firmware_update_btn_.EnableWindow(missing.empty());
        if (!busy && !image_blocker.empty()) {
            firmware_prerequisites_label_.SetWindowTextW(image_blocker.c_str());
            firmware_prerequisites_label_.SetEmphasized(true);
        } else if (missing.empty()) {
            firmware_prerequisites_label_.SetWindowTextW(
                L"Ready: Boot mode detected; Enter Boot is not needed. "
                L"Image, target, address and CRC32 are compatible. Next: Start Device Update.");
        } else {
            std::wstring text = busy ? L"Working: wait, or use Cancel.\r\nMissing: " :
                firmware_upgrade::feedback::CanNextStep(host_ready,update_target_ready,session_state_.f13_bootloader_ready)+L"\r\nMissing: ";
            for (std::size_t index = 0U; index < missing.size(); ++index) {
                if (index != 0U) {
                    text += L"; ";
                }
                text += missing[index];
            }
            firmware_prerequisites_label_.SetWindowTextW(text.c_str());
            firmware_prerequisites_label_.SetEmphasized(
                !busy && enter_bootloader_btn_.IsWindowEnabled());
        }
    } else {
        start_firmware_update_btn_.EnableWindow(FALSE);
        firmware_prerequisites_label_.SetWindowTextW(
            L"Custom Transfer is a non-destructive protocol test and does "
            L"not use a firmware BIN. Select CAN Upgrade to program an app.");
    }
    disconnect_btn_.EnableWindow(port_open || busy);
    load_binary_btn_.EnableWindow(!busy);

    start_log_btn_.EnableWindow(!log_recording_);
    stop_log_btn_.EnableWindow(log_recording_);
    export_summary_btn_.EnableWindow(!busy);
    UpdateFirmwareImageInfoText();
    UpdateTargetMetadataText();
    UpdateBusUi();
    if (!boot_profile || image_blocker.empty()) UpdateCompletionFeedback();
}

void CMainFrame::OnBnClickedConnectTarget() {
    if(IsIspTab()) {QueueUartAction(0U);return;}
    if (IsMiscTab()) { OnBridgeAction(0U); return; }
    if (IsI2cTab()) return;
    if (!session_state_.host_validated) {
        AppendLog(L"Connect blocked: MCU host is not validated.");
        return;
    }
    const std::wstring profile = SelectedProfile();
    firmware_upgrade::ui::CanSettings settings;
    std::wstring settings_error;
    if ((profile != L"Custom Transfer" && profile != L"CAN Upgrade") ||
        !SelectedCanSettings(&settings, &settings_error)) {
        AppendLog(L"Connect blocked: " +
                  (settings_error.empty() ?
                       std::wstring(L"unsupported target workflow.") :
                       settings_error));
        return;
    }
    if (profile == L"CAN Upgrade" && binary_inspection_.metadata_valid) {
        const std::uint8_t required_mode =
            settings.mode == firmware_upgrade::can_host::Mode::Classical ?
                0x01U : 0x02U;
        const std::uint32_t required_nominal =
            1UL << settings.nominal_bitrate_preset;
        const std::uint32_t required_data =
            settings.mode == firmware_upgrade::can_host::Mode::CanFd ?
                1UL << settings.data_bitrate_preset : 0U;
        if ((binary_inspection_.metadata.can_mode_mask & required_mode) == 0U ||
            (binary_inspection_.metadata.nominal_bitrate_mask &
             required_nominal) == 0U ||
            (required_data != 0U &&
             (binary_inspection_.metadata.data_bitrate_mask & required_data) ==
                 0U)) {
            AppendLog(L"Connect blocked: the selected CAN mode/bitrate is "
                      L"not supported by the loaded image metadata.");
            return;
        }
    }
    const bool queued = profile == L"CAN Upgrade" ?
        worker_session_.DiscoverF13Boot(settings) :
        worker_session_.ConnectF13Transfer(settings);
    if (!queued) {
        AppendLog(L"Connect blocked: another worker operation is active.");
        return;
    }

    worker_command_pending_ = true;
    SetProgress(0, L"Target discovery");
    StartOperationElapsedTimer();
    AppendLog(profile == L"CAN Upgrade" ?
        L"Queued read-only CAN Upgrade target discovery." :
        L"Queued Custom Transfer target discovery.");
    SaveUiSettings();
    UpdateUi();
}

void CMainFrame::OnBnClickedEnterBootloader() {
    if(IsIspTab()) {QueueUartAction(1U);return;}
    if (IsMiscTab()) { OnBridgeAction(1U); return; }
    if (IsI2cTab()) return;
    firmware_upgrade::ui::CanSettings settings;
    std::wstring settings_error;
    if (!session_state_.host_validated ||
        SelectedProfile() != L"CAN Upgrade" ||
        !SelectedCanSettings(&settings, &settings_error)) {
        AppendLog(L"Enter bootloader blocked: select CAN Upgrade "
                  L"after validating the MCU host.");
        return;
    }
    if (!worker_session_.EnterF13Bootloader(settings)) {
        AppendLog(L"Enter bootloader blocked: another operation is active.");
        return;
    }

    worker_command_pending_ = true;
    SetProgress(0, L"Enter bootloader");
    StartOperationElapsedTimer();
    AppendLog(L"Queued explicit application-to-bootloader transition.");
    SaveUiSettings();
    UpdateUi();
}

void CMainFrame::OnBnClickedTransferTest() {
    if(IsIspTab()) {QueueUartAction(UartTransferSelected()?3U:4U);return;}
    if (IsMiscTab()) { OnBridgeAction(3U); return; }
    if (IsI2cTab()) return;
    if(SelectedProfile()==L"CAN Upgrade") {
        if(session_state_.f13_bootloader_ready && worker_session_.VerifyInstalled()) {
            worker_command_pending_=true; StartOperationElapsedTimer(); UpdateUi();
        }
        return;
    }
    if (SelectedProfile() != L"Custom Transfer" ||
        !session_state_.target_validated ||
        session_state_.target_profile !=
            firmware_upgrade::ui::TargetProfile::F13Transfer) {
        AppendLog(L"Transfer test blocked: discover a Custom Transfer "
                  L"target first.");
        return;
    }

    if (!worker_session_.RunF13TransferTest()) {
        AppendLog(L"Transfer test blocked: another operation is active.");
        return;
    }

    worker_command_pending_ = true;
    SetProgress(0, L"Transfer test");
    StartOperationElapsedTimer();
    AppendLog(L"Queued non-destructive Custom Transfer test.");
    UpdateUi();
}

void CMainFrame::OnBnClickedCanTxTest() {
    if (IsMiscTab() || IsI2cTab() || IsUartTab()) return;
    if (session_state_.can_tx_test_active) {
        if (!worker_session_.StopCanTxTest()) {
            AppendLog(L"Bridge board self-test stop request was not accepted.");
            return;
        }
        AppendLog(L"Stopping bridge board self test.");
        return;
    }

    firmware_upgrade::ui::CanSettings settings;
    std::wstring error;
    if (!session_state_.host_validated ||
        !SelectedCanSettings(&settings, &error) ||
        settings.mode != firmware_upgrade::can_host::Mode::Classical) {
        AppendLog(L"Bridge board self test requires a validated host and Classical "
                  L"CAN at an advertised bitrate.");
        return;
    }
    if (!worker_session_.StartCanTxTest(settings)) {
        AppendLog(L"Bridge board self test blocked: another operation is active.");
        return;
    }

    worker_command_pending_ = true;
    SetProgress(0, L"Bridge board self test");
    StartOperationElapsedTimer();
    AppendLog(L"Queued bridge board self test: standard ID 0x123, DLC 8, "
              L"data 11 22 33 44 55 66 77 XX, period 1000 ms; "
              L"XX resets to 00 and increments for each frame.");
    UpdateUi();
}

void CMainFrame::OnBnClickedStartFirmwareUpdate() {
    if (IsMiscTab()) { OnBridgeAction(2U); return; }
    if (IsI2cTab()&&!IsI2cIsp()) return;
    StartDeviceUpdate(false);
}

void CMainFrame::StartDeviceUpdate(bool force_old_version) {
    if(IsIspTab()) {QueueUartAction(2U,force_old_version);return;}
    if(worker_command_pending_ || session_state_.busy) return;
    const firmware_upgrade::firmware_update::Profile profile =
        firmware_upgrade::firmware_update::FromDisplayName(SelectedProfile());
    if (profile != firmware_upgrade::firmware_update::Profile::CanUpgrade ||
        !session_state_.target_validated ||
        session_state_.target_profile !=
            firmware_upgrade::ui::TargetProfile::CanUpgrade ||
        !session_state_.f13_bootloader_ready) {
        AppendLog(L"Firmware update blocked: discover and validate the "
                  L"bootloader first.");
        UpdateUi();
        return;
    }
    if (binary_data_.empty()) {
        AppendLog(L"Firmware update blocked: no image is loaded.");
        UpdateUi();
        return;
    }

    firmware_upgrade::firmware_update::ImageInfo image_info;
    std::wstring error;
    if (!firmware_upgrade::firmware_update::ValidateImage(
            profile, binary_data_, &image_info, &error)) {
        AppendLog(L"Firmware update blocked: " + error);
        MessageBoxW(error.c_str(), L"Invalid Firmware Image",
                    MB_ICONERROR | MB_OK);
        UpdateUi();
        return;
    }
    std::uint32_t selected_application_start = 0U;
    if (!SelectedApplicationStart(&selected_application_start, &error) ||
        selected_application_start != image_info.application_start) {
        if (error.empty()) {
            error = L"App Start Override does not match image metadata.";
        }
        AppendLog(L"Firmware update blocked: " + error);
        UpdateUi();
        return;
    }
    if (!ImageMatchesTarget(image_info, session_state_.target_metadata)) {
        error = L"Loaded image metadata does not match the discovered MCU, "
                L"flash layout, CAN configuration, or update protocol.";
        AppendLog(L"Firmware update blocked: " + error);
        MessageBoxW(error.c_str(), L"Target/Image Mismatch",
                    MB_ICONERROR | MB_OK);
        UpdateUi();
        return;
    }

    if(!firmware_upgrade::device_version::Check(image_info,session_state_.target_metadata,force_old_version,&error)) {
        AppendLog(L"Firmware update blocked: "+error);
        MessageBoxW(error.c_str(),L"Version policy",MB_ICONWARNING|MB_OK); return;
    }
    if(force_old_version && !ConfirmForceOldFirmware()) {
        AppendLog(L"Force older firmware canceled before START; no permission retained."); return;
    }
    const auto approved_version=session_state_.target_metadata.app_version;
    std::wstringstream confirmation;
    confirmation << L"Program the validated " << image_info.mcu_family
                 << L" / " << image_info.mcu_part << L" image?\n\n"
                 << L"File: " << binary_path_ << L"\n"
                 << L"Workflow: " << image_info.profile_name << L"\n"
                 << L"Installed version: " << VersionText(approved_version)
                 << L"\nOperation: " << (force_old_version ? L"force upgrade old version firmware (this update only)" : L"Normal update")
                 << L"\nTarget: 0x" << std::hex << std::uppercase
                 << std::setfill(L'0') << std::setw(4)
                 << image_info.target_id << L"\nVersion: "
                 << VersionText(image_info.version) << L"\nBuild: "
                 << std::dec << image_info.build
                 << L"\nSize: " << std::dec << image_info.image_size
                 << L" bytes\nCRC32: 0x" << std::hex
                 << std::setw(8) << image_info.stored_crc32
                 << L"\n\n"
                 << L"This overwrites application flash 0x"
                 << image_info.application_start << L"-0x"
                 << image_info.application_end << L". "
                 << L"Keep power and CAN connected until END is "
                 << L"acknowledged.";
    if (MessageBoxW(confirmation.str().c_str(), L"Confirm Firmware Update",
                    MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
        AppendLog(L"Firmware update canceled before START.");
        return;
    }
    if (!worker_session_.ProgramF13Boot(binary_data_,force_old_version,approved_version)) {
        AppendLog(L"Firmware update blocked: another operation is active.");
        return;
    }

    worker_command_pending_ = true;
    SetProgress(0, L"Firmware update");
    StartOperationElapsedTimer();
    AppendLog(L"Queued validated CAN firmware update.");
    UpdateUi();
}

void CMainFrame::OnBnClickedDisconnect() {
    QueueDisconnect();
}
