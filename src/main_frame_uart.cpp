#include "main_frame.h"
#include "ui/overview_model.h"
#include "core/update_evidence.h"

std::wstring CMainFrame::BuildUartSummary() const {
    firmware_upgrade::isp::BusConfiguration selected;
    const auto bus=SelectedUartSettings(&selected)?firmware_upgrade::isp::BusText(SelectedIspContract(),selected):L"Invalid device bus settings";
    return std::wstring(L"Firmware Upgrade Tool / ")+firmware_upgrade::firmware_update::DisplayName(SelectedIspProfile())+L" device session\r\nHost: "+session_state_.host_summary+
        L"\r\nHost COM: "+session_state_.port_name+L" / "+std::to_wstring(session_state_.baudrate)+
        L"\r\nDevice bus: "+bus+
        L"; UART0 diagnostic Log\r\nWorkflow: "+(UartTransferSelected()?std::wstring(L"Custom Transfer"):std::wstring(firmware_upgrade::firmware_update::DisplayName(SelectedIspProfile())))+L"\r\nLive: "+
        (IsUartSelectionCurrent()?session_state_.uart_summary:L"not current / discover required")+
        L"\r\nImage file: "+binary_path_+L"\r\nBytes: "+std::to_wstring(binary_data_.size())+
        L"\r\nCRC32 calculated/stored: "+firmware_upgrade::evidence::Hex(binary_inspection_.calculated_crc32)+
        L" / "+firmware_upgrade::evidence::Hex(binary_inspection_.stored_crc32)+L"\r\nResult: "+summary_result_+
        L"\r\nElapsed ms: "+std::to_wstring(summary_elapsed_ms_)+L"\r\n";
}

bool CMainFrame::IsUartTab() const {
    return bus_tabs_.GetSafeHwnd()&&bus_tabs_.GetCurSel()==2;
}
bool CMainFrame::IsI2cIsp() const {return IsI2cTab()&&i2c_workflow_combo_.GetCurSel()!=2;}
bool CMainFrame::IsIspTab() const {return IsUartTab()||IsI2cIsp();}
std::uint8_t CMainFrame::SelectedIspContract() const {return IsI2cIsp()?2U:uart_workflow_combo_.GetCurSel()>=2?3U:1U;}
firmware_upgrade::firmware_update::Profile CMainFrame::SelectedIspProfile() const {
    using firmware_upgrade::firmware_update::Profile;
    return SelectedIspContract()==2U?Profile::I2cUpgrade:SelectedIspContract()==3U?Profile::XmodemUpgrade:Profile::UartUpgrade;
}
bool CMainFrame::UartTransferSelected() const {return IsI2cIsp()?i2c_workflow_combo_.GetCurSel()==1:(uart_workflow_combo_.GetCurSel()%2)==1;}
bool CMainFrame::SelectedUartSettings(firmware_upgrade::isp::BusConfiguration* configuration) const {
    if(!configuration)return false;
    *configuration={};
    if(IsI2cIsp()) {
        firmware_upgrade::i2c_host::Configuration i2c;
        if(!SelectedI2cSettings(&i2c,&configuration->address))return false;
        configuration->port=i2c.port;configuration->baud=i2c.speed==1U?400000U:100000U;
        return true;
    }
    const int selected=bus_port_combo_.GetCurSel();
    if(selected<0||bus_port_combo_.GetItemData(selected)==0xFFU)return false;
    configuration->port=static_cast<std::uint8_t>(bus_port_combo_.GetItemData(selected));
    CString text;uart_baud_combo_.GetWindowTextW(text);
    if(!firmware_upgrade::isp::ParseUartBaud(text.GetString(),&configuration->baud))return false;
    for(const auto& port:session_state_.uart_ports)if(port.id==configuration->port&&
        (port.flags&1U)&&firmware_upgrade::isp::UartRateSupported(port.rates,configuration->baud))return true;
    return false;
}
bool CMainFrame::IsUartSelectionCurrent() const {
    firmware_upgrade::isp::BusConfiguration selected;
    return session_state_.host_validated&&session_state_.uart_valid&&SelectedIspContract()==session_state_.isp_contract&&SelectedUartSettings(&selected)&&
        firmware_upgrade::isp::SameBus(selected,session_state_.uart_configuration)&&
        UartTransferSelected()==(session_state_.uart_target.mode==ISP_MODE_TRANSFER);
}
void CMainFrame::OnUartSettingsChanged() {
    if(loading_ui_settings_)return;
    session_state_.uart_valid=false;
    last_operation_error_.clear();last_update_result_=firmware_upgrade::feedback::Result::None;
    ValidateLoadedFirmwareImage(false);SaveUiSettings();UpdateUi();
}
void CMainFrame::UpdateUartUi() {
    using firmware_upgrade::visual::Tone;
    const bool busy=worker_command_pending_||session_state_.busy;
    RefreshBusPorts();
    firmware_upgrade::isp::BusConfiguration selected;
    const bool port=SelectedUartSettings(&selected);
    const bool host=session_state_.host_validated&&
        (session_state_.host_capabilities.feature_flags&(IsI2cIsp()?firmware_upgrade::bridge::kFeatureI2c:firmware_upgrade::bridge::kFeatureUart))&&
        !session_state_.can_started;
    const bool current=IsUartSelectionCurrent();const auto& t=session_state_.uart_target;
    const bool transfer=UartTransferSelected();
    const bool boot=current&&!transfer&&t.mode==ISP_MODE_BOOT&&!t.active;
    std::wstring mismatch;
    const bool image=binary_image_valid_&&binary_inspection_.metadata.profile==
        SelectedIspProfile();
    const bool match=image&&current&&!transfer&&firmware_upgrade::isp::Matches(binary_inspection_.metadata,t,&mismatch);
    std::uint32_t start=0U;std::wstring start_error;
    const bool address=image&&SelectedApplicationStart(&start,&start_error)&&start==binary_inspection_.metadata.application_start;
    const bool older=match&&t.valid&&t.version_control&&binary_inspection_.metadata.version<t.version;
    bus_tabs_.EnableWindow(!busy&&!session_state_.can_started);
    bus_port_combo_.EnableWindow(host&&!busy);uart_baud_combo_.EnableWindow(host&&!busy);
    uart_workflow_combo_.EnableWindow(!busy);app_start_edit_.EnableWindow(!busy&&!transfer);
    i2c_workflow_combo_.EnableWindow(!busy);i2c_speed_combo_.EnableWindow(host&&!busy);
    i2c_address_edit_.EnableWindow(host&&!busy);
    CWnd* other[]={&can_mode_combo_,&can_bitrate_combo_,&can_data_combo_,&can_tx_test_btn_,
        &can_filter_combo_,&can_filter_id_edit_,&can_filter_mask_edit_,&profile_combo_,&i2c_probe_btn_};
    for(auto* control:other)control->EnableWindow(FALSE);
    connect_target_btn_.SetWindowTextW(transfer?L"Discover Test Target":L"Discover Target");
    connect_target_btn_.EnableWindow(host&&port&&!busy);
    enter_bootloader_btn_.SetWindowTextW(L"Enter Boot");
    enter_bootloader_btn_.EnableWindow(host&&current&&!transfer&&t.mode==ISP_MODE_APP&&!busy);
    transfer_test_btn_.SetWindowTextW(transfer?L"Transfer Test":L"Verify Installed");
    transfer_test_btn_.EnableWindow(host&&current&&!busy&&(transfer||(boot&&t.valid)));
    start_firmware_update_btn_.SetWindowTextW(L"Start Device Update");
    start_firmware_update_btn_.EnableWindow(!busy&&host&&boot&&match&&address&&!older);
    force_old_firmware_btn_.ShowWindow(transfer?SW_HIDE:SW_SHOW);
    force_old_firmware_btn_.EnableWindow(!busy&&host&&boot&&match&&address&&older);
    compare_metadata_btn_.EnableWindow(!busy&&image&&current&&!transfer);
    load_binary_btn_.EnableWindow(!busy);disconnect_btn_.EnableWindow(session_state_.serial_open||busy);
    start_log_btn_.EnableWindow(!log_recording_);stop_log_btn_.EnableWindow(log_recording_);
    export_summary_btn_.EnableWindow(!busy);
    std::wstring next;
    Tone tone=Tone::Pending;
    if(busy)next=L"Working: wait, or use Cancel. Device UART1 is binary-only; UART0 remains the diagnostic Log.";
    else if(!host)next=L"[BLOCKED] Open + Probe a bridge App with the selected I2C/UART capability, select its bus port and keep CAN stopped.";
    else if(!port) {next=IsI2cIsp()?L"[BLOCKED] Select an advertised I2C speed and a7-bit address from0x08 to0x77 (not shifted).":
        L"[BLOCKED] Check Device baud (300..2000000) and the bridge port capability. Match the device Boot/App baud; Host Baud is independent.";tone=Tone::Error;}
    else if(transfer) {next=current?L"Ready: Transfer Test checks PING, ECHO and counters; Xmodem also checks CRC128 streaming. No BIN or Flash write.":L"Next: Discover Test Target with the matching standalone Transfer firmware.";tone=current?Tone::Ready:Tone::Pending;}
    else if(!binary_data_.empty()&&!image) {next=L"[BLOCKED] Check BIN update interface / protocol. Load the matching ISP _update.bin.\r\n"+binary_image_validation_error_;tone=Tone::Error;}
    else if(!current)next=L"Next: Discover Target (Boot or App mode). A loaded BIN is offline information only.";
    else if(t.active) {next=L"[BLOCKED] Device reports an active update. Preserve the Log; check Cancel cleanup or device reset, then Discover Target.";tone=Tone::Error;}
    else if(t.mode==ISP_MODE_APP)next=L"Next: Enter Boot. Then Start Device Update before the valid-App inactivity window expires.";
    else if(!image)next=L"Next: Load the matching ISP App _update.bin, then Start Device Update.";
    else if(!match) {next=L"[BLOCKED] "+mismatch+L"\r\nCheck Compare Metadata.";tone=Tone::Error;}
    else if(!address) {next=L"[BLOCKED] App Start Override must be Auto or the exact BIN address.";tone=Tone::Error;}
    else if(older) {next=L"[BLOCKED] Installed App requires same/newer version. For this downgrade, use force upgrade old version firmware.";tone=Tone::Error;}
    else {next=L"Ready: Start Device Update. Boot mode detected; Enter Boot is not needed.";tone=Tone::Ready;}
    firmware_prerequisites_label_.SetWindowTextW(next.c_str());firmware_prerequisites_label_.SetTone(tone);
    firmware_prerequisites_label_.SetEmphasized(!busy);
    operation_status_card_.SetWindowTextW(busy?L"[WORKING] UART device operation; Cancel is independent.":
        !last_operation_error_.empty()?last_operation_error_.c_str():L"Device bus and PC Host Link settings are independent. Device UART0 is diagnostic Log, not the ISP data channel.");
    operation_status_card_.SetTone(busy?Tone::Working:!last_operation_error_.empty()?Tone::Error:Tone::Neutral);
    if(IsI2cIsp())i2c_notes_.SetWindowTextW(L"Bridge IICA0 P62/P63 > device SCL/SDA; match the selected7-bit address and speed to Boot/App metadata. Shared ground and voltage-compatible pull-ups.\r\nDevice examples SCL/SDA: F13/F24 P62/P63; G16/G22 P60/P61; RH850 RIIC1 P8_1/P8_0. Diagnostic UART is independent.");
    else i2c_notes_.SetWindowTextW(L"Bridge UART1 TX P74/RX P75 > device RX/TX; share ground and verify voltage.\r\nDevice examples TX/RX: F13/F24 P74/P75; G16 P11/P10; G22 P00/P01; RH850 RLIN31 P0_5/P0_4. Diagnostic UART is independent.");
    UpdateFirmwareImageInfoText();UpdateTargetMetadataText();UpdateOverviewUi(false);UpdateCompletionFeedback();
    CRect client;GetClientRect(&client);LayoutControls(client.Width(),client.Height());
}
void CMainFrame::QueueUartAction(unsigned action,bool force) {
    if(worker_command_pending_||session_state_.busy)return;
    firmware_upgrade::isp::BusConfiguration configuration;
    if(!session_state_.host_validated||!SelectedUartSettings(&configuration))return;
    const bool transfer=UartTransferSelected();
    if(action&& !IsUartSelectionCurrent()) {AppendLog(L"UART operation blocked: discover the selected device first.");return;}
    if(action==2U) {
        firmware_upgrade::firmware_update::ImageInfo image;std::wstring error;
        std::uint32_t start=0U;
        if(transfer||session_state_.uart_target.mode!=ISP_MODE_BOOT||
            !firmware_upgrade::firmware_update::ValidateImage(SelectedIspProfile(),binary_data_,&image,&error)||
            !firmware_upgrade::isp::Matches(image,session_state_.uart_target,&error)||
            !SelectedApplicationStart(&start,&error)||start!=image.application_start) {
            AppendLog(L"UART update blocked: "+(error.empty()?L"Enter Boot and check App start.":error));return;
        }
        const auto& t=session_state_.uart_target;
        const bool blocked=t.valid&&t.version_control&&image.version<t.version;
        if(blocked&&!force) {AppendLog(L"UART update blocked: installed version policy requires same/newer.");return;}
        if(force&&(!blocked||!ConfirmForceOldFirmware()))return;
        const auto text=L"Program UART device App?\n\n"+image.mcu_family+L" / "+image.mcu_part+
            L"\nBytes: "+std::to_wstring(image.image_size)+L"\nFile: "+binary_path_+
            L"\n\nApplication Flash will be overwritten. Keep device power and UART1 connected until completion. UART0 Log remains separate.";
        if(MessageBoxW(text.c_str(),force?L"force upgrade old version firmware":L"Confirm UART Device Update",
            MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
    }
    if(worker_session_.UartAction(action,configuration,transfer,action==2U?binary_data_:std::vector<std::uint8_t>{},
        force,session_state_.uart_target.version,SelectedIspContract())) {
        worker_command_pending_=true;last_operation_error_.clear();
        last_update_result_=firmware_upgrade::feedback::Result::None;
        SetProgress(0,L"UART device operation");StartOperationElapsedTimer();SaveUiSettings();UpdateUi();
    }
}
