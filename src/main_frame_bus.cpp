#include "main_frame.h"
#include "resource.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

using firmware_upgrade::visual::Tone;
namespace {
void CardText(CStatusCard& card, const std::wstring& text, Tone tone) {
    CString old; card.GetWindowTextW(old);
    if (old != text.c_str()) card.SetWindowTextW(text.c_str());
    card.SetTone(tone);
}
}
bool CMainFrame::InitializeBusUi() {
    const DWORD label = WS_CHILD | WS_VISIBLE;
    const DWORD combo = label | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL;
    if (!bus_tabs_.Create(label | WS_TABSTOP | TCS_FIXEDWIDTH, CRect(), this, ID_BUS_TABS) ||
        !bus_port_label_.Create(L"Bridge port", label, CRect(), this, ID_BUS_PORT_LABEL) ||
        !bus_port_combo_.Create(combo, CRect(), this, ID_BUS_PORT_COMBO) ||
        !i2c_speed_label_.Create(L"I2C speed", label, CRect(), this, ID_I2C_SPEED_LABEL) ||
        !i2c_speed_combo_.Create(combo, CRect(), this, ID_I2C_SPEED_COMBO) ||
        !i2c_address_label_.Create(L"Slave (7-bit)", label, CRect(), this, ID_I2C_ADDRESS_LABEL) ||
        !i2c_address_edit_.Create(label | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                  CRect(), this, ID_I2C_ADDRESS_EDIT) ||
        !i2c_probe_btn_.Create(L"Probe G16 Reference", label | WS_TABSTOP | BS_PUSHBUTTON,
                               CRect(), this, ID_I2C_PROBE_BTN) ||
        !i2c_notes_.Create(L"", label, CRect(), this, ID_I2C_NOTES) ||
        !operation_status_card_.Create(L"", label, CRect(), this, ID_OPERATION_STATUS) ||
        !interface_heading_.Create(L"INTERFACE SETTINGS", label, CRect(), this, ID_INTERFACE_HEADING) ||
        !image_heading_.Create(L"SHARED IMAGE / METADATA", label, CRect(), this, ID_IMAGE_HEADING) ||
        !log_heading_.Create(L"SHARED LOG", label, CRect(), this, ID_LOG_HEADING) ||
        !InitializeOverviewUi())
        return false;
    bus_tabs_.InsertItem(0, L"CAN");
    bus_tabs_.InsertItem(1, L"I2C");
    bus_tabs_.InsertItem(2, L"Misc");
    bus_tabs_.SetCurSel(0);
    i2c_address_edit_.SetLimitText(4U);
    i2c_address_edit_.SetWindowTextW(L"0x09");
    CWnd* controls[] = {&bus_tabs_, &bus_port_label_, &bus_port_combo_,
        &i2c_speed_label_, &i2c_speed_combo_, &i2c_address_label_,
        &i2c_address_edit_, &i2c_probe_btn_, &i2c_notes_, &operation_status_card_,
        &interface_heading_, &image_heading_, &log_heading_};
    for (CWnd* control : controls) control->SetFont(&ui_font_);
    image_heading_.SetTone(Tone::Working);
    page_background_brush_.CreateSolidBrush(RGB(245,247,251));
    ready_background_brush_.CreateSolidBrush(firmware_upgrade::visual::Colors(Tone::Ready).background);
    error_background_brush_.CreateSolidBrush(firmware_upgrade::visual::Colors(Tone::Error).background);
    pending_background_brush_.CreateSolidBrush(firmware_upgrade::visual::Colors(Tone::Pending).background);
    neutral_background_brush_.CreateSolidBrush(firmware_upgrade::visual::Colors(Tone::Neutral).background);
    return true;
}
bool CMainFrame::IsI2cTab() const {
    return bus_tabs_.GetSafeHwnd() != nullptr && bus_tabs_.GetCurSel() == 1;
}
bool CMainFrame::IsMiscTab() const {
    return bus_tabs_.GetSafeHwnd() != nullptr && bus_tabs_.GetCurSel() == 2;
}
bool CMainFrame::IsI2cSelectionCurrent() const {
    firmware_upgrade::i2c_host::Configuration configuration;
    std::uint8_t address=0U;
    return session_state_.i2c_probe_validated && SelectedI2cSettings(&configuration,&address) &&
        configuration.port==session_state_.i2c_configuration.port &&
        configuration.speed==session_state_.i2c_configuration.speed && address==session_state_.i2c_address;
}
void CMainFrame::RefreshI2cRates() {
    const int selected = bus_port_combo_.GetCurSel();
    const std::uint8_t id = selected >= 0 ?
        static_cast<std::uint8_t>(bus_port_combo_.GetItemData(selected)) : 0xFFU;
    std::uint32_t rates = 0U;
    for (const auto& port : displayed_ports_) if (port.id == id) rates = port.rates;
    i2c_speed_combo_.ResetContent();
    const wchar_t* names[] = {L"100 kHz", L"400 kHz"};
    int preferred = 0;
    for (std::uint8_t i = 0U; i < 2U; ++i) if ((rates & (1UL << i)) != 0U) {
        const int index = i2c_speed_combo_.AddString(names[i]);
        i2c_speed_combo_.SetItemData(index, i);
        if (i == preferred_i2c_speed_) preferred = index;
    }
    if (i2c_speed_combo_.GetCount() > 0) i2c_speed_combo_.SetCurSel(preferred);
}
void CMainFrame::RefreshBusPorts() {
    const bool i2c = IsI2cTab();
    const auto& ports = i2c ? session_state_.i2c_ports : session_state_.can_ports;
    bool changed = displayed_i2c_ != i2c || displayed_ports_.size() != ports.size();
    if (!changed) for (std::size_t i = 0U; i < ports.size(); ++i)
        if (displayed_ports_[i].id != ports[i].id ||
            displayed_ports_[i].label != ports[i].label ||
            displayed_ports_[i].flags != ports[i].flags ||
            displayed_ports_[i].rates != ports[i].rates) changed = true;
    if (!changed && bus_port_combo_.GetCount() > 0) return;
    displayed_i2c_ = i2c; displayed_ports_ = ports;
    bus_port_combo_.ResetContent();
    int selected = -1;
    const auto preferred = i2c ? preferred_i2c_port_ : preferred_can_port_;
    for (const auto& port : ports) if ((port.flags & 1U) != 0U) {
        const std::wstring label = std::to_wstring(port.id) + L": " + port.label;
        const int index = bus_port_combo_.AddString(label.c_str());
        bus_port_combo_.SetItemData(index, port.id);
        if (port.id == preferred) selected = index;
    }
    if (bus_port_combo_.GetCount() == 0) {
        bus_port_combo_.AddString(session_state_.host_validated ?
            L"No supported port" : L"N/A - Probe bridge first");
        bus_port_combo_.SetItemData(0, 0xFFU);
    }
    bus_port_combo_.SetCurSel(selected >= 0 ? selected : 0);
    if (i2c) RefreshI2cRates();
}
void CMainFrame::UpdateBusUi() {
    if (bus_tabs_.GetSafeHwnd() == nullptr) return;
    if (IsMiscTab()) { UpdateMiscUi(); return; }
    RefreshBusPorts();
    const bool i2c = IsI2cTab();
    const bool busy = worker_command_pending_ || session_state_.busy;
    const bool mutable_bus = !busy && !session_state_.can_started;
    bus_tabs_.EnableWindow(mutable_bus);
    bus_port_combo_.EnableWindow(mutable_bus && session_state_.host_validated && !displayed_ports_.empty());
    i2c_speed_combo_.EnableWindow(mutable_bus && i2c && i2c_speed_combo_.GetCount() > 0);
    i2c_address_edit_.EnableWindow(mutable_bus);
    i2c_probe_btn_.EnableWindow(mutable_bus && session_state_.host_validated &&
        (session_state_.host_capabilities.feature_flags & firmware_upgrade::bridge::kFeatureI2c) != 0U &&
        i2c_speed_combo_.GetCount() > 0);
    const bool host = session_state_.host_validated;
    const bool i2c_current = IsI2cSelectionCurrent();
    if (busy)
        CardText(operation_status_card_, L"[WORKING] Operation active - wait, or cancel with Close COM", Tone::Working);
    else if (!last_operation_error_.empty())
        CardText(operation_status_card_, L"[ERROR] " + last_operation_error_, Tone::Error);
    else
        CardText(operation_status_card_, L"Green: ready | Amber: waiting | Red: blocked/error | Gray: N/A | Blue: offline BIN / working", Tone::Neutral);
    if (i2c) {
        can_mode_combo_.EnableWindow(FALSE); can_bitrate_combo_.EnableWindow(FALSE);
        can_data_combo_.EnableWindow(FALSE); can_tx_test_btn_.EnableWindow(FALSE);
        connect_target_btn_.EnableWindow(FALSE); enter_bootloader_btn_.EnableWindow(FALSE);
        transfer_test_btn_.EnableWindow(FALSE); start_firmware_update_btn_.EnableWindow(FALSE);
        app_start_edit_.EnableWindow(FALSE);
        std::wstring missing = L"[BLOCKED] ";
        if (!host) missing += L"Probe bridge; ";
        if (!i2c_current) missing += L"Probe G16 at current port/rate/address; ";
        if (binary_data_.empty()) missing += L"Load G16 app BIN; ";
        else if (binary_inspection_.metadata_available || binary_bridge_inspection_.available)
            missing += binary_image_validation_error_+L"; ";
        else if (!binary_i2c_inspection_.available || !binary_i2c_inspection_.matches)
            missing += L"Valid G16 CRC16 required; ";
        CardText(firmware_prerequisites_label_, missing +
            firmware_upgrade::g16_i2c::ProgrammingBlocker(), Tone::Error);
        CardText(i2c_notes_, L"Read-only discovery: 00, 55/repeated-start, inquiry, signature and area info. No scan, authentication, erase, write or app jump. Reference label/version are not authenticated identities.", Tone::Working);
    } else {
        CString prerequisites; firmware_prerequisites_label_.GetWindowTextW(prerequisites);
        firmware_prerequisites_label_.SetTone(prerequisites.Find(L"Ready:") == 0 ? Tone::Ready :
            (!binary_data_.empty() && !binary_image_valid_ && SelectedProfile() == L"CAN Upgrade" ? Tone::Error : Tone::Pending));
    }
    if (!i2c && session_state_.host_validated && session_state_.can_ports.empty()) {
        CWnd* unavailable[]={&can_mode_combo_,&can_bitrate_combo_,&can_data_combo_,&can_tx_test_btn_,
            &can_filter_combo_,&can_filter_id_edit_,&can_filter_mask_edit_,&profile_combo_,
            &connect_target_btn_,&enter_bootloader_btn_,&transfer_test_btn_,&start_firmware_update_btn_};
        for (auto* control:unavailable) control->EnableWindow(FALSE);
        CardText(firmware_prerequisites_label_,L"This host reports no CAN port. Bridge boot mode: use Misc; otherwise check advertised capabilities.",Tone::Pending);
    }
    UpdateOverviewUi(i2c_current);
    CRect client; GetClientRect(&client); LayoutControls(client.Width(), client.Height());
}
void CMainFrame::OnBusTabChanged(NMHDR*, LRESULT* result) {
    *result = 0;
    if (session_state_.busy || worker_command_pending_ || session_state_.can_started) {
        bus_tabs_.SetCurSel(active_tab_); return;
    }
    last_operation_error_.clear();
    ValidateLoadedFirmwareImage(false);
    SaveUiSettings(); UpdateUi();
}
void CMainFrame::OnBusPortChanged() {
    const int selection = bus_port_combo_.GetCurSel();
    if (selection < 0) return;
    const auto id = static_cast<std::uint8_t>(bus_port_combo_.GetItemData(selection));
    if (id == 0xFFU) return;
    if (IsI2cTab()) { preferred_i2c_port_ = id; RefreshI2cRates(); }
    else { preferred_can_port_ = id; QueueCanConfiguration(); }
    SaveUiSettings(); UpdateUi();
}
void CMainFrame::OnI2cProbe() {
    if (!IsI2cTab() || worker_command_pending_ || session_state_.busy) return;
    firmware_upgrade::i2c_host::Configuration configuration;
    std::uint8_t address = 0U;
    if (!SelectedI2cSettings(&configuration, &address)) {
        last_operation_error_ = L"I2C slave address must be an unshifted 7-bit value 0x08..0x77.";
        UpdateUi(); return;
    }
    if (worker_session_.ProbeI2c(configuration, address)) {
        worker_command_pending_ = true;
        last_operation_error_.clear();
        StartOperationElapsedTimer(); SaveUiSettings(); UpdateUi();
    }
}
bool CMainFrame::SelectedI2cSettings(
    firmware_upgrade::i2c_host::Configuration* configuration,
    std::uint8_t* address) const {
    const int port = bus_port_combo_.GetCurSel();
    const int speed = i2c_speed_combo_.GetCurSel();
    if (port < 0 || speed < 0 || bus_port_combo_.GetItemData(port) == 0xFFU)
        return false;
    CString text; i2c_address_edit_.GetWindowTextW(text);
    const std::wstring value = text.GetString();
    try {
        std::size_t consumed = 0U;
        if (value.empty() || value[0] == L'-' || value[0] == L'+' || value[0] == L' ')
            return false;
        const auto parsed = std::stoul(value, &consumed,
            value.rfind(L"0x", 0U) == 0U || value.rfind(L"0X", 0U) == 0U ? 16 : 10);
        if (consumed != value.size() || parsed < 8U || parsed > 0x77U) return false;
        *address = static_cast<std::uint8_t>(parsed);
    } catch (...) { return false; }
    configuration->port = static_cast<std::uint8_t>(bus_port_combo_.GetItemData(port));
    configuration->speed = static_cast<std::uint8_t>(i2c_speed_combo_.GetItemData(speed));
    return true;
}
void CMainFrame::OnI2cSettingsChanged() {
    if (loading_ui_settings_ || operation_status_card_.GetSafeHwnd() == nullptr ||
        detailed_trace_check_.GetSafeHwnd() == nullptr) return;
    const int speed = i2c_speed_combo_.GetCurSel();
    if (speed >= 0) preferred_i2c_speed_ =
        static_cast<std::uint8_t>(i2c_speed_combo_.GetItemData(speed));
    SaveUiSettings(); UpdateUi();
}
void CMainFrame::LayoutInterfaceControls(int x, int y, int width, int height, int row) {
    const bool i2c = IsI2cTab();
    const bool misc = IsMiscTab();
    CWnd* can_controls[] = {&can_mode_label_, &can_mode_combo_, &can_bitrate_label_,
        &can_bitrate_combo_, &can_data_label_, &can_data_combo_, &can_tx_test_btn_,
        &can_tx_test_help_, &can_filter_label_, &can_filter_combo_, &can_filter_id_label_,
        &can_filter_id_edit_, &can_filter_mask_label_, &can_filter_mask_edit_,
        &profile_label_, &profile_combo_};
    for (CWnd* control : can_controls) control->ShowWindow(i2c || misc ? SW_HIDE : SW_SHOW);
    CWnd* i2c_controls[] = {&i2c_speed_label_, &i2c_speed_combo_, &i2c_address_label_,
        &i2c_address_edit_, &i2c_probe_btn_};
    for (CWnd* control : i2c_controls) control->ShowWindow(i2c ? SW_SHOW : SW_HIDE);
    bus_port_label_.ShowWindow(misc ? SW_HIDE : SW_SHOW);
    bus_port_combo_.ShowWindow(misc ? SW_HIDE : SW_SHOW);
    connect_target_btn_.ShowWindow(i2c ? SW_HIDE : SW_SHOW);
    enter_bootloader_btn_.ShowWindow(i2c ? SW_HIDE : SW_SHOW);
    transfer_test_btn_.ShowWindow(i2c ? SW_HIDE : SW_SHOW);
    i2c_notes_.ShowWindow(i2c || misc ? SW_SHOW : SW_HIDE);
    const int step = row + 2;
    const auto move = [&](CWnd& c, int left, int top, int w, int h) {
        if (c.GetSafeHwnd()) c.MoveWindow(left, top, w, h, FALSE);
    };
    const auto pair = [&](CWnd& label, CWnd& value, int top) {
        CClientDC dc(&label); CFont* old=dc.SelectObject(&ui_font_);
        CString text; label.GetWindowTextW(text);
        const int lw=dc.GetTextExtent(text).cx+10; dc.SelectObject(old);
        move(label,x,top+3,lw,row-2);
        const bool combo=(value.GetStyle() & CBS_DROPDOWNLIST)==CBS_DROPDOWNLIST;
        move(value,x+lw+3,top,width-lw-3,combo ? 220 : row-2);
    };
    if (misc) {
        move(connect_target_btn_,x,y,width,row);
        move(enter_bootloader_btn_,x,y+step,width,row);
        move(transfer_test_btn_,x,y+step*2,width,row);
        move(disconnect_btn_,x,y+step*3,width,row);
        move(i2c_notes_,x,y+step*4,width,height-step*4);
    } else if (i2c) {
        pair(bus_port_label_,bus_port_combo_,y);
        pair(i2c_speed_label_,i2c_speed_combo_,y+step);
        pair(i2c_address_label_,i2c_address_edit_,y+step*2);
        move(i2c_probe_btn_,x,y+step*3,width,row);
        move(disconnect_btn_,x,y+step*4,width,row);
        move(i2c_notes_,x,y+step*5,width,height-step*5);
    } else {
        pair(bus_port_label_,bus_port_combo_,y);
        pair(can_mode_label_,can_mode_combo_,y+step);
        pair(can_bitrate_label_,can_bitrate_combo_,y+step*2);
        pair(can_data_label_,can_data_combo_,y+step*3);
        pair(can_filter_label_,can_filter_combo_,y+step*4);
        const int half=(width-4)/2;
        move(can_filter_id_label_,x,y+step*5+3,24,row-2);
        move(can_filter_id_edit_,x+26,y+step*5,half-26,row-2);
        move(can_filter_mask_label_,x+half+4,y+step*5+3,40,row-2);
        move(can_filter_mask_edit_,x+half+46,y+step*5,width-half-46,row-2);
        pair(profile_label_,profile_combo_,y+step*6);
        move(connect_target_btn_,x,y+step*7,width,row);
        const int third=(width-4)/2;
        move(enter_bootloader_btn_,x,y+step*8,third,row);
        move(transfer_test_btn_,x+third+4,y+step*8,width-third-4,row);
        move(can_tx_test_btn_,x,y+step*9,width-23,row);
        move(can_tx_test_help_,x+width-20,y+step*9+3,20,row);
        move(disconnect_btn_,x,y+step*10,width,row);
    }
}
