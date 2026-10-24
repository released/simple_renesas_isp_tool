#include "main_frame.h"
#include "ui/log_view.h"

#include <iterator>
#include <string>

namespace {

constexpr const wchar_t* kIniSectionUi = L"ui";

} // namespace

void CMainFrame::InitializeSettingsFile() {
    wchar_t module_path[MAX_PATH] = {};
    const DWORD length =
        GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    std::wstring directory = L".";
    if (length > 0U) {
        directory.assign(module_path, module_path + length);
        const std::size_t position = directory.find_last_of(L"\\/");
        if (position != std::wstring::npos) {
            directory.resize(position);
        }
    }

    ini_path_ = directory + L"\\FirmwareUpgradeTool.ini";
    WriteIniValue(L"tool", L"Firmware Upgrade Tool");
}

std::wstring CMainFrame::ReadIniValue(const wchar_t* key,
                                      const wchar_t* fallback) const {
    wchar_t buffer[1024] = {};
    GetPrivateProfileStringW(
        kIniSectionUi, key, fallback, buffer,
        static_cast<DWORD>(std::size(buffer)), ini_path_.c_str());
    return std::wstring(buffer);
}

void CMainFrame::WriteIniValue(const wchar_t* key,
                               const std::wstring& value) {
    if (ini_path_.empty()) return;
    if (WritePrivateProfileStringW(kIniSectionUi, key, value.c_str(), ini_path_.c_str()))
        return;
    const DWORD error = GetLastError();
    if (!ini_write_error_reported_) {
        ini_write_error_reported_ = true;
        last_operation_error_ = L"INI write failed: " + ini_path_ +
            L" (Windows error " + std::to_wstring(error) + L").";
        last_error_guidance_.clear();
        AppendLog(last_operation_error_);
    }
}

void CMainFrame::ApplyComboSelection(CComboBox& combo,
                                     const std::wstring& value) {
    if (value.empty()) {
        return;
    }
    const int index = combo.FindStringExact(-1, value.c_str());
    if (index >= 0) {
        combo.SetCurSel(index);
    }
}

void CMainFrame::LoadUiSettings() {
    loading_ui_settings_ = true;
    const auto tab=ReadIniValue(L"bus_tab", L"CAN");
    bus_tabs_.SetCurSel(tab == L"Misc" ? 3 : tab == L"UART" ? 2 : tab == L"I2C" ? 1 : 0);
    preferred_uart_port_=static_cast<std::uint8_t>(GetPrivateProfileIntW(kIniSectionUi,L"uart_port",0,ini_path_.c_str()));
    ApplyComboSelection(uart_workflow_combo_,ReadIniValue(L"uart_workflow",L"UART ISP"));
    uart_baud_combo_.SetWindowTextW(ReadIniValue(L"device_uart_baud",L"115200").c_str());
    ApplyComboSelection(i2c_workflow_combo_,ReadIniValue(L"i2c_workflow",L"I2C ISP"));
    preferred_can_port_ = static_cast<std::uint8_t>(
        GetPrivateProfileIntW(kIniSectionUi, L"can_port", 0, ini_path_.c_str()));
    preferred_i2c_port_ = static_cast<std::uint8_t>(
        GetPrivateProfileIntW(kIniSectionUi, L"i2c_port", 0, ini_path_.c_str()));
    preferred_i2c_speed_ = ReadIniValue(L"i2c_speed", L"0") == L"1" ? 1U : 0U;
    i2c_address_edit_.SetWindowTextW(ReadIniValue(L"i2c_address", L"0x20").c_str());
    preferred_com_port_ = ReadIniValue(L"com_port", L"");
    selected_com_instance_=ReadIniValue(L"com_device_hint",L"");
    verified_com_port_=ReadIniValue(L"com_verified_port",L"");
    ApplyComboSelection(baud_combo_, ReadIniValue(L"baud", L"115200"));
    ApplyComboSelection(
        can_bitrate_combo_, ReadIniValue(L"can_bitrate", L"500K"));
    ApplyComboSelection(
        can_mode_combo_, ReadIniValue(L"can_mode", L"Classical CAN"));
    ApplyComboSelection(
        can_data_combo_, ReadIniValue(L"can_data_bitrate", L"N/A"));
    std::wstring stored_filter =
        ReadIniValue(L"can_filter", L"Metadata/default");
    if (stored_filter == L"Profile default") {
        stored_filter = L"Metadata/default";
    }
    ApplyComboSelection(can_filter_combo_, stored_filter);
    can_filter_id_edit_.SetWindowTextW(
        ReadIniValue(L"can_filter_id", L"0x701").c_str());
    can_filter_mask_edit_.SetWindowTextW(
        ReadIniValue(L"can_filter_mask", L"0x7FF").c_str());
    std::wstring stored_profile =
        ReadIniValue(L"target_profile", L"Custom Transfer");
    if (stored_profile == L"F13 Boot") {
        stored_profile = L"CAN Upgrade";
    } else if (stored_profile == L"F13 Transfer") {
        stored_profile = L"Custom Transfer";
    }
    ApplyComboSelection(profile_combo_, stored_profile);
    app_start_edit_.SetWindowTextW(
        ReadIniValue(L"application_start", L"Auto").c_str());
    detailed_trace_check_.SetCheck(
        ReadIniValue(L"detailed_trace", L"0") == L"1" ?
            BST_CHECKED : BST_UNCHECKED);
    log_auto_scroll_=ReadIniValue(L"log_auto_scroll",L"1")!=L"0";
    log_auto_scroll_check_.SetCheck(log_auto_scroll_ ? BST_CHECKED : BST_UNCHECKED);

    binary_path_ = ReadIniValue(L"binary_path", L"");
    if (!binary_path_.empty() &&
        !LoadBinaryFromPath(binary_path_, true)) {
        file_path_label_.SetWindowTextW(
            (L"Update image: " + binary_path_ + L" (missing)").c_str());
        binary_data_.clear();
        binary_inspection_ = {};
        binary_image_valid_ = false;
        binary_image_validation_error_ = L"firmware file is missing";
    }
    loading_ui_settings_ = false;
}

void CMainFrame::SaveUiSettings() {
    if (loading_ui_settings_ || ini_path_.empty()) return;
    WriteIniValue(L"bus_tab", IsMiscTab() ? L"Misc" : IsUartTab()?L"UART":IsI2cTab() ? L"I2C" : L"CAN");
    WriteIniValue(L"uart_port",std::to_wstring(preferred_uart_port_));
    CString uart_workflow;uart_workflow_combo_.GetWindowTextW(uart_workflow);
    WriteIniValue(L"uart_workflow",uart_workflow.GetString());
    CString device_baud;uart_baud_combo_.GetWindowTextW(device_baud);
    WriteIniValue(L"device_uart_baud",device_baud.GetString());
    CString i2c_workflow;i2c_workflow_combo_.GetWindowTextW(i2c_workflow);WriteIniValue(L"i2c_workflow",i2c_workflow.GetString());
    WriteIniValue(L"can_port", std::to_wstring(preferred_can_port_));
    WriteIniValue(L"i2c_port", std::to_wstring(preferred_i2c_port_));
    WriteIniValue(L"i2c_speed", std::to_wstring(preferred_i2c_speed_));
    CString i2c_address; i2c_address_edit_.GetWindowTextW(i2c_address);
    WriteIniValue(L"i2c_address", i2c_address.GetString());
    const auto selected_port=SelectedComPort();
    if(!selected_port.empty()) WriteIniValue(L"com_port", selected_port);
    WriteIniValue(L"com_device_hint",selected_com_instance_);
    WriteIniValue(L"com_verified_port",verified_com_port_);
    WriteIniValue(L"baud", std::to_wstring(SelectedBaudrate()));
    WriteIniValue(L"can_bitrate", SelectedCanBitrate());
    CString text;
    can_mode_combo_.GetWindowTextW(text);
    WriteIniValue(L"can_mode", text.GetString());
    can_data_combo_.GetWindowTextW(text);
    WriteIniValue(L"can_data_bitrate", text.GetString());
    can_filter_combo_.GetWindowTextW(text);
    WriteIniValue(L"can_filter", text.GetString());
    can_filter_id_edit_.GetWindowTextW(text);
    WriteIniValue(L"can_filter_id", text.GetString());
    can_filter_mask_edit_.GetWindowTextW(text);
    WriteIniValue(L"can_filter_mask", text.GetString());
    WriteIniValue(L"target_profile", SelectedProfile());
    CString application_start;
    app_start_edit_.GetWindowTextW(application_start);
    WriteIniValue(L"application_start", application_start.GetString());
    WriteIniValue(L"binary_path", binary_path_);
    WriteIniValue(
        L"detailed_trace",
        detailed_trace_check_.GetCheck() == BST_CHECKED ? L"1" : L"0");
    WriteIniValue(L"log_auto_scroll",log_auto_scroll_ ? L"1" : L"0");
}

void CMainFrame::OnLogAutoScroll() {
    log_auto_scroll_=log_auto_scroll_check_.GetCheck()==BST_CHECKED;
    if (log_auto_scroll_) firmware_upgrade::log_view::ScrollToEnd(log_edit_.GetSafeHwnd());
    SaveUiSettings();
}
