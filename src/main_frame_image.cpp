#include "main_frame.h"
#include <afxdlgs.h>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
namespace {
std::wstring Hex32(std::uint32_t value) { std::wstringstream s; s << L"0x" << std::hex << std::uppercase << std::setfill(L'0') << std::setw(8) << value; return s.str(); }
std::wstring VersionText(std::uint32_t version) {
    if (version == 0xFFFFFFFFU) {
        return L"not installed";
    }
    return std::to_wstring((version >> 16U) & 0xFFU) + L"." +
           std::to_wstring((version >> 8U) & 0xFFU) + L"." +
           std::to_wstring(version & 0xFFU);
}

void AppendMaskName(std::wstring* text, const wchar_t* name) {
    if (!text->empty()) {
        *text += L"/";
    }
    *text += name;
}

std::wstring BitrateMaskText(std::uint32_t mask, bool data_rate) {
    std::wstring text;
    const wchar_t* const nominal_names[] = {L"125K", L"250K", L"500K", L"800K", L"1M"};
    const wchar_t* const data_names[] = {L"1M", L"2M", L"4M", L"5M", L"8M"};
    const std::size_t count = data_rate ? std::size(data_names) :
                                          std::size(nominal_names);
    for (std::size_t index = 0U; index < count; ++index) {
        if ((mask & (1UL << index)) != 0U) {
            AppendMaskName(&text, data_rate ? data_names[index] :
                                            nominal_names[index]);
        }
    }
    if (text.empty()) {
        return L"N/A";
    }
    return text;
}

std::wstring CanModeText(std::uint8_t mask) {
    std::wstring text;
    if ((mask & 0x01U) != 0U) {
        text = L"Classical";
    }
    if ((mask & 0x02U) != 0U) {
        AppendMaskName(&text, L"CAN FD");
    }
    return text.empty() ? L"unknown" : text;
}

}
void CMainFrame::OnBnClickedLoadBinary() {
    CFileDialog dialog(
        TRUE, L"bin", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        L"Firmware Update Images (*.bin)|*.bin|"
        L"All Files (*.*)|*.*||",
        this);
    if (dialog.DoModal() != IDOK) {
        return;
    }

    const std::wstring path = dialog.GetPathName().GetString();
    if (!LoadBinaryFromPath(path, false)) {
        MessageBoxW(
            L"Failed to open update image.", L"Load Update Image",
            MB_ICONERROR | MB_OK);
    }
}

bool CMainFrame::LoadBinaryFromPath(const std::wstring& path, bool silent) {
    last_update_result_=firmware_upgrade::feedback::Result::None;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (!silent) {
            AppendLog(L"Firmware load failed: " + path);
        }
        return false;
    }

    binary_data_.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
    if (binary_data_.empty()) {
        binary_image_valid_ = false;
        binary_image_validation_error_ = L"firmware image is empty";
        binary_path_.clear();
        binary_inspection_ = {};
        binary_i2c_inspection_ = {};
        binary_bridge_inspection_ = {};
        file_path_label_.SetWindowTextW(L"Update image: none (empty file)");
        if (!silent) {
            AppendLog(L"Firmware load failed: empty file.");
        }
        SaveUiSettings();
        UpdateUi();
        return false;
    }

    binary_path_ = path;
    binary_i2c_inspection_ = firmware_upgrade::g16_i2c::InspectImage(binary_data_);
    binary_bridge_inspection_ = firmware_upgrade::bridge_update::Inspect(binary_data_);
    binary_inspection_ =
        firmware_upgrade::firmware_update::InspectImage(binary_data_);
    if (!IsMiscTab() && !IsI2cTab() && !IsUartTab() && binary_inspection_.metadata_valid &&
        binary_inspection_.metadata.profile==firmware_upgrade::firmware_update::Profile::CanUpgrade) {
        if (!session_state_.can_started) {
            const int workflow_index =
                profile_combo_.FindStringExact(-1, L"CAN Upgrade");
            const int filter_index =
                can_filter_combo_.FindStringExact(-1, L"Metadata/default");
            if (workflow_index >= 0) {
                profile_combo_.SetCurSel(workflow_index);
            }
            if (filter_index >= 0) {
                can_filter_combo_.SetCurSel(filter_index);
            }
        }
        can_filter_id_edit_.SetWindowTextW(
            Hex32(binary_inspection_.metadata.response_id).c_str());
        can_filter_mask_edit_.SetWindowTextW(
            (binary_inspection_.metadata.can_id_format == 1U ?
                 L"0x1FFFFFFF" : L"0x7FF"));
    }
    file_path_label_.SetWindowTextW(
        (L"Update image: " + binary_path_).c_str());
    AppendLog(
        std::wstring(silent ? L"Firmware restored: " : L"Firmware loaded: ") +
        binary_path_ + L", size=" +
        std::to_wstring(binary_data_.size()) + L" bytes" +
        ((!IsI2cTab() || binary_inspection_.metadata_available || binary_bridge_inspection_.available) && binary_inspection_.checksum_available ?
            L", calculated CRC32=" +
                Hex32(binary_inspection_.calculated_crc32) +
                L", stored CRC32=" +
                Hex32(binary_inspection_.stored_crc32) +
                L", checksum=" +
                std::wstring(binary_inspection_.checksum_matches ?
                                 L"MATCH" : L"MISMATCH") :
            L", checksum trailer unavailable") + L".");
    ValidateLoadedFirmwareImage(true);
    if (IsI2cTab()) AppendLog(BuildI2cSummary());
    SaveUiSettings();
    UpdateUi();
    return true;
}

void CMainFrame::ValidateLoadedFirmwareImage(bool emit_log) {
    last_update_result_=firmware_upgrade::feedback::Result::None;
    firmware_upgrade::image_policy::Policy policy;
    const bool known=binary_inspection_.metadata_available || binary_bridge_inspection_.available;
    if (known && (binary_data_.size()<0x200U || !firmware_upgrade::image_policy::Parse(binary_data_.data()+0x100U,&policy) ||
        !firmware_upgrade::image_policy::Allows(policy, IsMiscTab()?IP_ROLE_BRIDGE_APP:IP_ROLE_DEVICE_APP,
            IsMiscTab()||IsUartTab()?IP_UART:(IsI2cTab()?IP_I2C:IP_CAN), &binary_image_validation_error_))) {
        binary_image_valid_=false;
        if (!policy.role) binary_image_validation_error_=L"Missing or unsupported common image update policy.";
        if (emit_log) AppendLog(binary_image_validation_error_);
        return;
    }
    if (IsMiscTab()) {
        binary_image_valid_=binary_bridge_inspection_.valid;
        binary_image_validation_error_=binary_bridge_inspection_.error;
        if (emit_log) AppendLog(binary_image_valid_ ? L"Bridge app BUR1 manifest / CRC32 valid (offline)." :
            L"Bridge app rejected: "+binary_image_validation_error_);
        return;
    }
    if (IsI2cTab()&&!IsI2cIsp()) {
        binary_image_valid_ = false;
        binary_image_validation_error_ = firmware_upgrade::g16_i2c::ProgrammingBlocker();
        if (emit_log && !binary_data_.empty())
            AppendLog(L"G16 reference CRC16 inspected (range excludes last four bytes; stored LE16 at size-4). Raw BIN has no identity/version/start manifest. " + binary_image_validation_error_);
        return;
    }
    if (binary_data_.empty()) {
        binary_image_valid_ = false;
        binary_image_validation_error_ = L"firmware image is empty";
        return;
    }

    firmware_upgrade::firmware_update::ImageInfo image_info;
    binary_image_validation_error_.clear();
    binary_image_valid_ = firmware_upgrade::firmware_update::ValidateImage(
        IsIspTab()?SelectedIspProfile():firmware_upgrade::firmware_update::Profile::CanUpgrade,
        binary_data_, &image_info,
        &binary_image_validation_error_);
    if (binary_image_valid_) {
        std::uint32_t selected_application_start = 0U;
        if (!SelectedApplicationStart(
                &selected_application_start,
                &binary_image_validation_error_)) {
            binary_image_valid_ = false;
        } else if (selected_application_start !=
                   image_info.application_start) {
            binary_image_valid_ = false;
            binary_image_validation_error_ =
                L"App Start Override does not match image metadata (" +
                Hex32(image_info.application_start) + L").";
        }
    }

    if (!emit_log) {
        return;
    }
    if (binary_image_valid_) {
        AppendLog(
            L"Firmware image passed " + image_info.profile_name +
            L" validation; size=" +
            std::to_wstring(image_info.image_size) + L" bytes.");
    } else {
        AppendLog(L"Firmware image is not update-ready: " +
                  binary_image_validation_error_);
    }
}

void CMainFrame::UpdateFirmwareImageInfoText() {
    if (firmware_field_values_[0].GetSafeHwnd() == nullptr) {
        return;
    }
    if (binary_bridge_inspection_.available || (IsMiscTab() && !binary_inspection_.metadata_available)) {
        UpdateBridgeImageFields();
        firmware_upgrade::image_policy::Policy policy;
        if (binary_data_.size()>=0x200U && firmware_upgrade::image_policy::Parse(binary_data_.data()+0x100U,&policy)) {
            firmware_field_values_[kFirmwareFieldUpdatePolicy].SetWindowTextW(
                (L"HOST APP | "+firmware_upgrade::image_policy::Transports(policy.transports)).c_str());
            firmware_field_values_[kFirmwareFieldBootAbi].SetWindowTextW(
                (VersionText(policy.minimum_boot_version)+L" / "+Hex32(policy.irq_abi)).c_str());
        }
        return;
    }
    if (IsI2cTab() && !IsI2cIsp() && !binary_inspection_.metadata_available) { UpdateI2cImageFields(IsI2cSelectionCurrent()); return; }
    firmware_field_labels_[kFirmwareFieldCalculatedCrc].SetWindowTextW(L"Calculated CRC32");
    firmware_field_labels_[kFirmwareFieldStoredCrc].SetWindowTextW(L"Stored CRC32");
    for (std::size_t index = 0U; index < kFirmwareFieldCount; ++index) {
        firmware_field_values_[index].SetWindowTextW(L"N/A");
    }
    if (binary_data_.empty()) {
        return;
    }

    std::wstring text =
        std::to_wstring(binary_inspection_.image_size) + L" bytes (" +
        Hex32(static_cast<std::uint32_t>(binary_inspection_.image_size)) +
        L")";
    firmware_field_values_[kFirmwareFieldBinSize].SetWindowTextW(
        text.c_str());
    if (!binary_inspection_.checksum_available) {
        firmware_field_values_[kFirmwareFieldChecksumResult].SetWindowTextW(
            L"UNAVAILABLE");
    } else {
        firmware_field_values_[kFirmwareFieldCalculatedCrc].SetWindowTextW(
            Hex32(binary_inspection_.calculated_crc32).c_str());
        firmware_field_values_[kFirmwareFieldStoredCrc].SetWindowTextW(
            Hex32(binary_inspection_.stored_crc32).c_str());
        firmware_field_values_[kFirmwareFieldChecksumResult].SetWindowTextW(
            binary_inspection_.checksum_matches ? L"MATCH" : L"MISMATCH");
    }

    if (!binary_inspection_.metadata_valid) {
        firmware_field_values_[kFirmwareFieldMetadataStatus].SetWindowTextW(
            L"INVALID (see log)");
        return;
    }

    const firmware_upgrade::firmware_update::ImageInfo& image =
        binary_inspection_.metadata;
    firmware_field_values_[kFirmwareFieldVersionControl].SetWindowTextW(
        image.policy.version_control ? L"ENABLED | same / newer" : L"DISABLED | any version");
    firmware_field_values_[kFirmwareFieldInstalledVersionPolicy].SetWindowTextW(
        session_state_.target_validated && session_state_.target_profile==firmware_upgrade::ui::TargetProfile::CanUpgrade ?
            (session_state_.target_metadata.version_policy==3U ? L"ENABLED | same / newer" :
                session_state_.target_metadata.version_policy==1U ? L"DISABLED | any version" : L"RECOVERY | no valid App") : L"Discover Target required");
    firmware_field_values_[kFirmwareFieldUpdatePolicy].SetWindowTextW(
        (std::wstring(image.policy.role==IP_ROLE_DEVICE_APP?L"DEVICE APP | ":L"HOST APP | ")+
            firmware_upgrade::image_policy::Transports(image.policy.transports)+
            firmware_upgrade::image_policy::DeviceUartText(image.policy)).c_str());
    firmware_field_values_[kFirmwareFieldBootAbi].SetWindowTextW(
        (VersionText(image.policy.minimum_boot_version)+L" / "+Hex32(image.policy.irq_abi)).c_str());
    firmware_field_values_[kFirmwareFieldMetadataStatus].SetWindowTextW(
        L"VALID");
    firmware_field_values_[kFirmwareFieldMcu].SetWindowTextW(
        (image.mcu_family + L" / " + image.mcu_part).c_str());
    firmware_field_values_[kFirmwareFieldAppVersion].SetWindowTextW(
        VersionText(image.version).c_str());
    firmware_field_values_[kFirmwareFieldBuild].SetWindowTextW(
        std::to_wstring(image.build).c_str());
    firmware_field_values_[kFirmwareFieldTarget].SetWindowTextW(
        Hex32(image.target_id).c_str());
    firmware_field_values_[kFirmwareFieldProtocolFormat].SetWindowTextW(
        ((image.profile==firmware_upgrade::firmware_update::Profile::UartUpgrade ? std::wstring(L"UART ISP ") : std::to_wstring(image.protocol_id)+L".") +
         std::to_wstring(image.protocol_version) + L" / " +
         std::to_wstring(image.format) + L" / " +
         (image.checksum_type == 1U ? L"CRC32" : L"Unknown")).c_str());
    firmware_field_values_[kFirmwareFieldAppRange].SetWindowTextW(
        (Hex32(image.application_start) + L" / " +
         Hex32(image.application_end)).c_str());
    firmware_field_values_[kFirmwareFieldImageCapacity].SetWindowTextW(
        (std::to_wstring(image.image_size) + L" / " +
         std::to_wstring(image.application_capacity) + L" B").c_str());
    firmware_field_values_[kFirmwareFieldFlashBoot].SetWindowTextW(
        (std::to_wstring(image.code_flash_size) + L" / " +
         std::to_wstring(image.boot_reserved_size) + L" B").c_str());
    firmware_field_values_[kFirmwareFieldMinimumErase].SetWindowTextW(
        (std::to_wstring(image.minimum_image_size) + L" / " +
         std::to_wstring(image.erase_block_size) + L" B").c_str());
    firmware_field_values_[kFirmwareFieldCanRates].SetWindowTextW(
        (CanModeText(image.can_mode_mask) + L"; " +
         BitrateMaskText(image.nominal_bitrate_mask, false) + L" / " +
         BitrateMaskText(image.data_bitrate_mask, true)).c_str());
    firmware_field_values_[kFirmwareFieldCanIds].SetWindowTextW(
        ((image.can_id_format == 1U ? L"EXT " : L"STD ") +
         Hex32(image.request_id) + L" / " +
         Hex32(image.response_id)).c_str());
    if(image.profile==firmware_upgrade::firmware_update::Profile::UartUpgrade||image.profile==firmware_upgrade::firmware_update::Profile::XmodemUpgrade||image.profile==firmware_upgrade::firmware_update::Profile::I2cUpgrade) {
        firmware_field_values_[kFirmwareFieldCanRates].SetWindowTextW(
            (L"UART / "+std::to_wstring(image.policy.device_uart_baud)+L" / 8-N-1").c_str());
        firmware_field_values_[kFirmwareFieldCanIds].SetWindowTextW(L"PC > bridge UART1 > device UART1");
        firmware_field_values_[kFirmwareFieldProtocolFormat].SetWindowTextW((std::wstring(firmware_upgrade::firmware_update::DisplayName(image.profile))+L" / format6 / CRC32").c_str());
        if(image.profile==firmware_upgrade::firmware_update::Profile::I2cUpgrade) {
            firmware_field_values_[kFirmwareFieldCanRates].SetWindowTextW((L"I2C / "+std::to_wstring(image.nominal_bitrate_mask/1000U)+L" kHz").c_str());
            firmware_field_values_[kFirmwareFieldCanIds].SetWindowTextW((L"7-bit address 0x"+Hex32(image.data_bitrate_mask).substr(8U)).c_str());
        }
        const bool live=IsUartSelectionCurrent();const auto& t=session_state_.uart_target;
        firmware_field_values_[kFirmwareFieldInstalledVersionPolicy].SetWindowTextW(!live?L"Discover Target required":
            !t.valid?L"RECOVERY | no valid App":t.version_control?L"ENABLED | same / newer":L"DISABLED | any version");
    }
}

void CMainFrame::UpdateTargetMetadataText() {
    if(IsIspTab()) {
        target_metadata_label_.SetWindowTextW(IsUartSelectionCurrent()?session_state_.uart_summary.c_str():
            L"ISP device: Discover Target; UART0 is diagnostic Log. Select the matching I2C/UART/Xmodem workflow.");return;
    }
    if (IsI2cTab() || IsMiscTab()) return;
    if (target_metadata_label_.GetSafeHwnd() == nullptr) {
        return;
    }
    if (!session_state_.target_validated ||
        session_state_.target_profile ==
            firmware_upgrade::ui::TargetProfile::None) {
        target_metadata_label_.SetWindowTextW(
            SelectedProfile() == L"Custom Transfer" ?
                L"Custom Transfer: no firmware BIN is used. Discover a test "
                L"target that implements the Custom Transfer v1 contract." :
                L"Device metadata: not available (Discover Target required)");
        return;
    }
    if (session_state_.target_profile ==
        firmware_upgrade::ui::TargetProfile::F13Transfer) {
        target_metadata_label_.SetWindowTextW(
            (L"Custom Transfer target: " + session_state_.target_summary +
             L"\r\nDiagnostic workflow only; firmware and flash-layout "
             L"metadata are not part of this contract.").c_str());
        return;
    }

    const firmware_upgrade::f13_boot::TargetInfo& target =
        session_state_.target_metadata;
    std::wstring text = L"LIVE device layout: app " + Hex32(target.application_start) +
        L", capacity " + std::to_wstring(target.application_capacity) +
        L" B; flash " + std::to_wstring(target.code_flash_size) +
        L" B; min/erase " + std::to_wstring(target.minimum_image_size) + L"/" +
        std::to_wstring(target.erase_block_size) + L" B. CAN " +
        CanModeText(target.can_mode_mask) + L" " +
        BitrateMaskText(target.nominal_bitrate_mask, false) + L"/" +
        BitrateMaskText(target.data_bitrate_mask, true) +
        (target.can_id_format == 1U ? L"   Extended IDs " :
                                     L"   Standard IDs ") +
        Hex32(target.request_id) + L"/" + Hex32(target.response_id);
    target_metadata_label_.SetWindowTextW(text.c_str());
}


