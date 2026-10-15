#include "main_frame.h"
#include "build_info.generated.h"
#include <cstring>
#include <iomanip>
#include <sstream>

namespace {
using firmware_upgrade::visual::Tone;
std::wstring Wide(const char* text) { return std::wstring(text,text+strlen(text)); }
std::wstring Hex(std::uint32_t n) {
    std::wstringstream s; s<<L"0x"<<std::hex<<std::uppercase<<std::setfill(L'0')<<std::setw(8)<<n; return s.str();
}
void Text(CStatusCard& card,const std::wstring& text,Tone tone) {
    card.SetWindowTextW(text.c_str()); card.SetTone(tone);
}
}
void CMainFrame::UpdateBridgeImageFields() {
    firmware_field_labels_[kFirmwareFieldCalculatedCrc].SetWindowTextW(L"Calculated CRC32");
    firmware_field_labels_[kFirmwareFieldStoredCrc].SetWindowTextW(L"Stored CRC32");
    for (auto& value:firmware_field_values_) value.SetWindowTextW(L"N/A");
    if (binary_data_.empty()) return;
    const auto& image=binary_bridge_inspection_;
    const auto set=[&](FirmwareFieldIndex index,const std::wstring& text) { firmware_field_values_[index].SetWindowTextW(text.c_str()); };
    set(kFirmwareFieldBinSize,std::to_wstring(image.size)+L" B");
    set(kFirmwareFieldMetadataStatus,image.valid ? L"VALID" : L"INVALID - HOST APP required");
    if (image.available) {
        set(kFirmwareFieldCalculatedCrc,Hex(image.calculated_crc)); set(kFirmwareFieldStoredCrc,Hex(image.stored_crc));
        set(kFirmwareFieldChecksumResult,image.crc_matches ? L"MATCH" : L"MISMATCH");
    }
    if (!image.valid) return;
    const auto& m=image.metadata;
    set(kFirmwareFieldMcu,Wide(m.family)+L" / "+Wide(m.part));
    set(kFirmwareFieldAppVersion,std::to_wstring((m.version>>16)&255)+L"."+
        std::to_wstring((m.version>>8)&255)+L"."+std::to_wstring(m.version&255));
    set(kFirmwareFieldBuild,std::to_wstring(m.build)); set(kFirmwareFieldTarget,Hex(m.board_id)+L" HOST");
    set(kFirmwareFieldProtocolFormat,L"Bridge UART contract3 / BUR1");
    set(kFirmwareFieldAppRange,Hex(m.app_start)+L".."+Hex(m.app_start+m.image_size-1));
    set(kFirmwareFieldImageCapacity,std::to_wstring(m.image_size)+L" / "+std::to_wstring(m.capacity)+L" B");
    set(kFirmwareFieldFlashBoot,std::to_wstring(m.flash_size)+L" / "+std::to_wstring(m.boot_size)+L" B");
    set(kFirmwareFieldMinimumErase,std::to_wstring(m.minimum_size)+L" / "+std::to_wstring(m.erase_size)+L" B");
    set(kFirmwareFieldCanRates,L"UART / "+std::to_wstring(m.uart_baud));
    set(kFirmwareFieldCanIds,L"PC -> HOST BRIDGE");
}
void CMainFrame::UpdateMiscUi() {
    const bool busy=worker_command_pending_ || session_state_.busy;
    const bool info=session_state_.host_validated && session_state_.bridge_info_valid;
    const auto& bridge=session_state_.bridge_info;
    const bool boot=info && bridge.mode==BU_MODE_BOOT;
    const bool compatible=info && binary_bridge_inspection_.valid &&
        Bu_MatchesDevice(&binary_bridge_inspection_.metadata,&bridge);
    bus_tabs_.EnableWindow(!busy && !session_state_.can_started);
    app_start_edit_.EnableWindow(FALSE);
    connect_target_btn_.SetWindowTextW(L"Read Bridge Update Info");
    enter_bootloader_btn_.SetWindowTextW(L"Enter Bridge Boot (UART)");
    transfer_test_btn_.SetWindowTextW(L"Run Validated Bridge App");
    start_firmware_update_btn_.SetWindowTextW(L"Update HOST APP");
    connect_target_btn_.EnableWindow(!busy && info);
    enter_bootloader_btn_.EnableWindow(!busy && info && !boot);
    transfer_test_btn_.EnableWindow(!busy && boot && (bridge.flags&BU_FLAG_VALID_APP));
    start_firmware_update_btn_.EnableWindow(!busy && boot && compatible && (bridge.flags&BU_FLAG_FLASH_READY));
    disconnect_btn_.EnableWindow(session_state_.serial_open || busy);
    std::wstring reason;
    if (!session_state_.host_validated) reason=L"Open + Probe Host first.";
    else if (!info) reason=L"This bridge firmware does not advertise safe self-update. Install its split boot/app with a debugger first.";
    else if (!boot) reason=L"Enter Bridge Boot first; CAN/I2C target programming is not used here.";
    else if (!binary_bridge_inspection_.valid) reason=binary_data_.empty() ? L"Load a HOST APP _bridge_update.bin." :
        (binary_inspection_.metadata_available?binary_image_validation_error_:binary_bridge_inspection_.error);
    else if (!compatible) reason=L"HOST/image identity, flash geometry or minimum boot version mismatch.";
    else if (!(bridge.flags&BU_FLAG_FLASH_READY)) reason=L"Boot flash backend is not ready. Programming blocked.";
    else reason=L"Ready: HOST APP only, UART, CRC32 commit. Boot protected. No automatic RUN.";
    Text(firmware_prerequisites_label_,reason,reason.rfind(L"Ready:",0)==0 ? Tone::Ready :
        (!binary_data_.empty() && !compatible?Tone::Error:Tone::Pending));
    Text(i2c_notes_,L"MISC: HOST BRIDGE\nPC -> bridge UART\nNo external device\n\n1. Probe host\n2. Enter bridge boot\n3. Load HOST APP BIN\n4. Update HOST APP\n5. Run app explicitly\n\nBoot / option bytes / reserved flash protected. CRC is integrity, not authentication.",Tone::Working);
    target_metadata_label_.SetWindowTextW(info ?
        (L"HOST BRIDGE geometry: app "+Hex(bridge.app_start)+L", capacity "+std::to_wstring(bridge.capacity)+
         L" B; boot "+std::to_wstring(bridge.boot_size)+L" B; UART "+std::to_wstring(bridge.uart_baud)+L". External device metadata is not used.").c_str() :
        L"HOST BRIDGE self-update: N/A. Support is discovered from the connected board, never inferred from COM name.");
    Text(operation_status_card_,busy ? L"[WORKING] HOST BRIDGE UART operation" :
        (!last_operation_error_.empty() ? L"[ERROR] "+last_operation_error_ : L"MISC: update HOST BRIDGE APP, not external CAN / I2C device"),
        busy ? Tone::Working : !last_operation_error_.empty() ? Tone::Error : Tone::Pending);
    UpdateOverviewUi(false);
    CRect client; GetClientRect(&client); LayoutControls(client.Width(),client.Height());
}
void CMainFrame::OnBridgeAction(unsigned action) {
    if (!IsMiscTab() || worker_command_pending_ || session_state_.busy || !session_state_.bridge_info_valid) return;
    if (action==1U || action==2U || action==3U) {
        std::wstringstream text;
        text<<L"Destination: HOST BRIDGE (not external device)\nCOM: "<<session_state_.port_name
            <<L"\nBoard: "<<Wide(session_state_.bridge_info.part)<<L"\nAction: "
            <<(action==1U ? L"Enter bridge boot; disconnect device transports" : action==2U ? L"Overwrite HOST APP flash over UART" : L"Run validated HOST APP");
        if (action==2U) {
            if (!binary_bridge_inspection_.valid || !Bu_MatchesDevice(&binary_bridge_inspection_.metadata,&session_state_.bridge_info)) return;
            text<<L"\nBIN: "<<binary_path_<<L"\nApp start: "<<Hex(binary_bridge_inspection_.metadata.app_start)
                <<L"\nSize: "<<binary_data_.size()<<L" B\nBoot protected; keep power/UART connected.";
        }
        if (MessageBoxW(text.str().c_str(),L"Confirm HOST BRIDGE action",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
    }
    if (worker_session_.BridgeAction(action,action==2U ? binary_data_ : std::vector<std::uint8_t>{})) {
        worker_command_pending_=true; last_operation_error_.clear();
        StartOperationElapsedTimer(); SetProgress(0,L"Host bridge UART"); UpdateUi();
    }
}
std::wstring CMainFrame::BuildBridgeSummary() const {
    std::wstringstream text;
    text<<L"FirmwareUpgradeTool "<<FIRMWARE_UPGRADE_TOOL_VERSION<<L"\r\nTAB: Misc\r\n"
        <<L"Destination: HOST BRIDGE APP via UART; external CAN/I2C device is inactive.\r\n"
        <<L"Selected COM: "<<SelectedComPort()<<L"\r\nHistorical host summary: "<<summary_host_<<L"\r\n";
    if (session_state_.host_validated && session_state_.bridge_info_valid) {
        const auto& d=session_state_.bridge_info;
        text<<L"LIVE bridge: "<<Wide(d.family)<<L" / "<<Wide(d.part)<<L", board "<<Hex(d.board_id)
            <<L", mode "<<(d.mode==BU_MODE_BOOT ? L"BOOT" : L"APP")<<L"\r\n"
            <<L"LIVE app start/capacity/erase: "<<Hex(d.app_start)<<L" / "<<d.capacity<<L" / "<<d.erase_size<<L"\r\n"
            <<L"LIVE installed app CRC valid: "<<((d.flags&BU_FLAG_VALID_APP) ? L"yes" : L"no")<<L"\r\n";
    } else text<<L"LIVE bridge metadata: N/A (probe required / disconnected)\r\n";
    text<<L"OFFLINE image: "<<binary_path_<<L"\r\nSize: "<<binary_data_.size()<<L" B\r\n"
        <<L"BUR1 valid: "<<(binary_bridge_inspection_.valid ? L"yes" : L"no")<<L"\r\n"
        <<L"Calculated/stored CRC32: "<<Hex(binary_bridge_inspection_.calculated_crc)<<L" / "
        <<Hex(binary_bridge_inspection_.stored_crc)<<L"\r\n"
        <<L"Result: "<<summary_result_<<L"\r\nCRC is integrity, not authentication.\r\n";
    return text.str();
}
