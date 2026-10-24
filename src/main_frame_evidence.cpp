#include "main_frame.h"
#include "resource.h"
#include "build_info.generated.h"
#include "core/update_evidence.h"
#include "core/preflight_summary.h"
#include "core/operation_guidance.h"
#include "core/device_version_policy.h"
#include <afxdlgs.h>
#include <shellapi.h>

namespace {
class ForcePasswordDialog : public CDialog {
public:
    explicit ForcePasswordDialog(CWnd* parent) : CDialog(IDD_FORCE_OLD_PASSWORD,parent) {}
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog(); edit_.SubclassDlgItem(ID_FORCE_OLD_PASSWORD,this);
        edit_.SetLimitText(16); CenterWindow(); edit_.SetFocus(); return FALSE;
    }
    void OnOK() override {
        CString password; edit_.GetWindowTextW(password);
        const bool accepted=firmware_upgrade::device_version::PasswordMatches(password.GetString());
        edit_.SetWindowTextW(L""); password.Empty();
        if(!accepted) {
            MessageBoxW(L"Incorrect maintenance password. No update has started.",
                L"force upgrade old version firmware",MB_OK|MB_ICONWARNING);
            edit_.SetFocus(); return;
        }
        CDialog::OnOK();
    }
private:
    CEdit edit_;
};
class MetadataDialog : public CDialog {
public:
    MetadataDialog(const std::vector<firmware_upgrade::evidence::Difference>& rows,CWnd* parent,
                   const std::wstring& title=L"",bool details_only=false)
        : CDialog(IDD_METADATA_DIFFERENCE,parent), rows_(rows), title_(title), details_only_(details_only) {}
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog(); list_.SubclassDlgItem(ID_METADATA_DIFFERENCE_LIST,this);
        if(!title_.empty()) SetWindowTextW(title_.c_str());
        list_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
        CRect r; list_.GetClientRect(&r);
        list_.InsertColumn(0,L"Field",LVCFMT_LEFT,r.Width()*(details_only_?30:26)/100);
        list_.InsertColumn(1,details_only_?L"Displayed value":title_.empty()?L"Loaded image":L"Current / image",
            LVCFMT_LEFT,r.Width()*(details_only_?67:27)/100);
        if(!details_only_) {
            list_.InsertColumn(2,title_.empty()?L"Live device":L"Required / live",LVCFMT_LEFT,r.Width()*27/100);
            list_.InsertColumn(3,L"Check",LVCFMT_LEFT,r.Width()*18/100);
        }
        for(const auto& row:rows_) {
            const int i=list_.InsertItem(list_.GetItemCount(),row.field.c_str());
            list_.SetItemText(i,1,row.image.c_str());
            if(!details_only_){list_.SetItemText(i,2,row.live.c_str());list_.SetItemText(i,3,row.result.c_str());}
        }
        CenterWindow(); return TRUE;
    }
    BOOL OnNotify(WPARAM w,LPARAM l,LRESULT* result) override {
        const auto* header=reinterpret_cast<const NMHDR*>(l);
        if(!details_only_ && header && header->hwndFrom==list_.GetSafeHwnd() && header->code==NM_CUSTOMDRAW) {
            auto* draw=reinterpret_cast<NMLVCUSTOMDRAW*>(l);
            if(draw->nmcd.dwDrawStage==CDDS_PREPAINT) {*result=CDRF_NOTIFYITEMDRAW;return TRUE;}
            if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT && draw->nmcd.dwItemSpec<rows_.size()) {
                const auto& state=rows_[draw->nmcd.dwItemSpec].result;
                draw->clrText=state==L"MISMATCH"?RGB(170,0,35):state==L"CHANGE"?RGB(130,80,0):RGB(0,100,65);
                draw->clrTextBk=state==L"MISMATCH"?RGB(255,235,238):state==L"CHANGE"?RGB(255,247,218):RGB(232,255,243);
                *result=CDRF_DODEFAULT;return TRUE;
            }
        }
        return CDialog::OnNotify(w,l,result);
    }
private:
    std::vector<firmware_upgrade::evidence::Difference> rows_;
    CListCtrl list_;
    std::wstring title_;
    bool details_only_;
};
}
bool CMainFrame::ConfirmForceOldFirmware() {
    ForcePasswordDialog dialog(this); return dialog.DoModal()==IDOK;
}
void CMainFrame::UpdateCompletionFeedback() {
    namespace feedback=firmware_upgrade::feedback;
    if (worker_command_pending_ || session_state_.busy || !last_operation_error_.empty() ||
        last_update_result_==feedback::Result::None || (IsI2cTab()&&!IsI2cIsp())) return;
    if(last_update_result_==feedback::Result::UartAppConfirmed||last_update_result_==feedback::Result::UartAppConfirmedRxWarning) {if(!IsIspTab()||UartTransferSelected())return;}
    else if (feedback::IsDevice(last_update_result_) ? IsMiscTab() || IsUartTab() || SelectedProfile()!=L"CAN Upgrade" : !IsMiscTab()) return;
    const auto view=feedback::Describe(last_update_result_);
    const auto tone=view.warning ? firmware_upgrade::visual::Tone::Pending : firmware_upgrade::visual::Tone::Ready;
    operation_status_card_.SetWindowTextW(view.status.c_str()); operation_status_card_.SetTone(tone);
    firmware_prerequisites_label_.SetWindowTextW(view.compact.c_str());
    firmware_prerequisites_label_.SetTone(tone);
    overview_tooltip_.UpdateTipText((view.status+L"\r\n"+view.next).c_str(),&firmware_prerequisites_label_);
}
void CMainFrame::OnForceOldFirmware() {
    if(IsMiscTab() || (IsI2cTab()&&!IsI2cIsp())) return;
    StartDeviceUpdate(true);
}
void CMainFrame::OnCancelOperation() {
    if(worker_session_.RequestCancel()) AppendLog(L"Cancel requested. Worker will attempt ABORT/stop cleanup; COM remains owned by the worker.");
}
void CMainFrame::OnCompareMetadata() {
    std::vector<firmware_upgrade::evidence::Difference> rows;
    if(IsMiscTab() && binary_bridge_inspection_.valid && session_state_.bridge_info_valid)
        rows=firmware_upgrade::evidence::Compare(binary_bridge_inspection_.metadata,session_state_.bridge_info);
    else if(IsIspTab()&&IsUartSelectionCurrent()&&binary_inspection_.metadata_valid) {
        const auto& i=binary_inspection_.metadata;const auto& t=session_state_.uart_target;
        rows.push_back({L"MCU",i.mcu_family+L" / "+i.mcu_part,t.family+L" / "+t.part,
            i.mcu_family==t.family&&i.mcu_part==t.part?L"MATCH":L"MISMATCH"});
        const auto row=[&](const wchar_t* name,std::uint32_t a,std::uint32_t b) {
            rows.push_back({name,firmware_upgrade::evidence::Hex(a),firmware_upgrade::evidence::Hex(b),a==b?L"MATCH":L"MISMATCH"});
        };
        row(L"Target",i.target_id,t.target);row(L"App start",i.application_start,t.start);
        row(L"App capacity",i.application_capacity,t.capacity);row(L"Flash size",i.code_flash_size,t.flash);
        row(L"Boot reserve",i.boot_reserved_size,t.boot);row(L"Erase block",i.erase_block_size,t.erase);
        row(L"IRQ ABI",i.policy.irq_abi,t.irq_abi);row(L"Device rate",t.contract==2U?i.nominal_bitrate_mask:i.policy.device_uart_baud,t.baud);
        if(t.contract==2U)row(L"I2C7-bit address (selected session)",i.data_bitrate_mask,t.address);
        row(L"Protocol ID",i.protocol_id,t.contract==2U?0x49U:t.contract==3U?0x58U:0x55U);
        rows.push_back({L"Minimum Boot",firmware_upgrade::evidence::Hex(i.policy.minimum_boot_version),
            firmware_upgrade::evidence::Hex(t.boot_version),i.policy.minimum_boot_version<=t.boot_version?L"MATCH":L"MISMATCH"});
        rows.push_back({L"App version",firmware_upgrade::evidence::Hex(i.version),
            t.valid?firmware_upgrade::evidence::Hex(t.version):L"No valid App",i.version==t.version?L"MATCH":L"CHANGE"});
    }
    else if(!IsUartTab() && !IsI2cTab() && !IsMiscTab() && binary_inspection_.metadata_valid && session_state_.target_validated)
        rows=firmware_upgrade::evidence::Compare(binary_inspection_.metadata,session_state_.target_metadata);
    if(rows.empty()) {AppendLog(L"Compare Metadata requires a valid image and live metadata for the selected destination.");return;}
    MetadataDialog dialog(rows,this); dialog.DoModal();
}
std::vector<firmware_upgrade::evidence::Difference> CMainFrame::BuildBinDetails() const {
    std::vector<firmware_upgrade::evidence::Difference> rows;
    if(binary_data_.empty())return rows;
    rows.push_back({L"Loaded BIN (offline)",binary_path_,L"",L""});
    for(std::size_t field=0;field<kFirmwareFieldCount;++field) {
        CString name,value;
        firmware_field_labels_[field].GetWindowTextW(name);
        firmware_field_values_[field].GetWindowTextW(value);
        rows.push_back({name.GetString(),value.GetString(),L"",L""});
    }
    CString guidance;firmware_prerequisites_label_.GetWindowTextW(guidance);
    rows.push_back({L"Current workflow guidance",guidance.GetString(),L"",L""});
    return rows;
}
void CMainFrame::OnBinDetails() {
    if(worker_command_pending_ || session_state_.busy)return;
    const auto rows=BuildBinDetails();
    if(rows.empty())return;
    MetadataDialog dialog(rows,this,L"BIN Details - offline file / Device LIVE policy labeled",true);
    dialog.DoModal();
}
void CMainFrame::SaveUpdateReport(const firmware_upgrade::ui::WorkerEvent& event) {
    if(!event.report_json_path.empty()) AppendLog(L"Automatic JSON update report: "+event.report_json_path);
    if(!event.report_html_path.empty()) {
        latest_report_path_=event.report_html_path;
        AppendLog(L"Automatic HTML update report: "+latest_report_path_);
    }
    if(!event.text.empty()) AppendLog(L"Automatic report warning: "+event.text);
    UpdateUi();
}
void CMainFrame::OnOpenLatestReport() {
    if(latest_report_path_.empty()) return;
    const auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(m_hWnd,L"open",latest_report_path_.c_str(),nullptr,nullptr,SW_SHOWNORMAL));
    if(result<=32) AppendLog(L"Report open failed (Shell error "+std::to_wstring(result)+L"): "+latest_report_path_);
}
void CMainFrame::OnErrorDetails() {
    if(last_operation_error_.empty()) return;
    MessageBoxW((last_error_guidance_.empty()?firmware_upgrade::guidance::Format(last_operation_error_):
        last_error_guidance_).c_str(),L"Operation Error / Suggested Checks",MB_OK|MB_ICONINFORMATION);
}
void CMainFrame::OnPreflight() {
    using namespace firmware_upgrade;
    if(worker_command_pending_ || session_state_.busy) return;
    std::vector<evidence::Difference> rows;
    const auto gate=[&](const wchar_t* field,bool ok,const std::wstring& current,const std::wstring& action) {
        evidence::Gate(rows,field,ok,current,action);
    };
    gate(L"Live host",session_state_.host_validated,session_state_.host_summary,L"Open + Probe Host; COM description is not identity");
    const bool transfer = IsIspTab()?UartTransferSelected():!IsMiscTab() && !IsI2cTab() && SelectedProfile()==L"Custom Transfer";
    if (!transfer)
        gate(L"Loaded BIN",!binary_data_.empty(),std::to_wstring(binary_data_.size())+L" bytes",L"Load Update Image for the selected destination");
    if(IsMiscTab()) {
        const bool info=session_state_.host_validated && session_state_.bridge_info_valid;
        const auto& d=session_state_.bridge_info;
        gate(L"Destination",info && d.mode==BU_MODE_BOOT,L"HOST BRIDGE APP / UART",L"Enter Bridge Boot; external device flash is not selected");
        gate(L"Image / CRC32",binary_bridge_inspection_.valid,binary_bridge_inspection_.error,L"Valid _bridge_update.bin / full CRC32");
        gate(L"Flash backend",info && (d.flags&BU_FLAG_FLASH_READY),info?L"Live descriptor":L"Unavailable",L"Boot FLASH_READY required");
        gate(L"Update UART",info && session_state_.baudrate==d.uart_baud,
            std::to_wstring(session_state_.baudrate),L"Use live Boot descriptor rate (reference host: 115200)");
        if(info && binary_bridge_inspection_.valid) {
            gate(L"Host image contract",Bu_MatchesDevice(&binary_bridge_inspection_.metadata,&d)!=0,
                L"Loaded image vs live descriptor",L"Board, partition, role, UART and minimum Boot must match");
            const auto diff=evidence::Compare(binary_bridge_inspection_.metadata,d);
            rows.insert(rows.end(),diff.begin(),diff.end());
        }
    } else if(IsI2cTab()&&!IsI2cIsp()) {
        gate(L"Reference discovery",IsI2cSelectionCurrent(),session_state_.i2c_summary,L"Probe the explicitly selected port/rate/address");
        gate(L"Programming adapter",false,L"Read-only reference workflow",L"Erase/write is not enabled for this adapter");
    } else if(IsIspTab()) {
        gate(L"Bridge bus support",(session_state_.host_capabilities.feature_flags&(IsI2cIsp()?bridge::kFeatureI2c:bridge::kFeatureUart))!=0U,
            L"Advertised bridge capability",L"Use a bridge App with the selected I2C/UART forwarding");
        isp::BusConfiguration uart_configuration;
        const bool settings=SelectedUartSettings(&uart_configuration);
        gate(L"Device bus",settings,settings?isp::BusText(SelectedIspContract(),uart_configuration):L"Invalid settings",L"Match the selected device rate/address to Boot/App; UART0 is Log only");
        gate(L"Live ISP device",IsUartSelectionCurrent(),session_state_.uart_summary,L"Discover Target / Discover Test Target");
        if(!transfer) {
            gate(L"ISP BIN / CRC32",binary_image_valid_,binary_image_validation_error_,L"Load the matching interface/protocol _update.bin");
            gate(L"Device Boot",IsUartSelectionCurrent()&&session_state_.uart_target.mode==ISP_MODE_BOOT,
                L"Live UART mode",L"Enter Boot when App is running");
            std::wstring error;
            gate(L"BIN / live contract",binary_image_valid_&&IsUartSelectionCurrent()&&isp::Matches(binary_inspection_.metadata,session_state_.uart_target,&error),
                error,L"Compare MCU, Flash layout, IRQ ABI, minimum Boot and device baud");
        }
    } else if (transfer) {
        gate(L"Workflow",true,L"Custom Transfer / non-destructive",L"No firmware BIN or Flash operation is used");
        ui::CanSettings settings; std::wstring error;
        const bool selected=SelectedCanSettings(&settings,&error);
        can_host::Configuration config;
        config.mode=settings.mode;config.nominal_bitrate_preset=settings.nominal_bitrate_preset;
        config.data_bitrate_preset=settings.data_bitrate_preset;
        gate(L"Selected CAN settings",selected && session_state_.host_validated &&
            can_host::IsConfigurationSupported(config,session_state_.host_capabilities,&error),
            error,L"Select the target's compiled Classical or FD rate");
        const bool live=session_state_.target_validated && session_state_.target_profile==ui::TargetProfile::F13Transfer;
        gate(L"Live test target",live,session_state_.target_summary,L"Discover Test Target");
        if (selected && settings.mode==can_host::Mode::CanFd)
            gate(L"FD64 echo capability",live &&
                (session_state_.transfer_metadata.capabilities&CT_FD64_CAPABILITY),
                L"Live Custom Transfer capability bit6",L"FD Transfer firmware must advertise ECHO_FD64");
    } else {
        const bool image=binary_image_valid_ && binary_inspection_.metadata_valid;
        const bool live=session_state_.target_validated && session_state_.target_profile==ui::TargetProfile::CanUpgrade;
        gate(L"Workflow",SelectedProfile()==L"CAN Upgrade",SelectedProfile(),L"CAN Upgrade; Custom Transfer is a non-destructive test");
        gate(L"Live device",live,session_state_.target_summary,L"Discover Target with CAN Upgrade");
        gate(L"Device Boot",live && session_state_.f13_bootloader_ready,L"Live device mode",L"Enter Boot before programming");
        gate(L"Image / CRC32",image,binary_image_validation_error_,L"Valid device _update.bin / full CRC32");
        ui::CanSettings settings; std::wstring error;
        const bool selected=SelectedCanSettings(&settings,&error);
        gate(L"Host burst support",selected && session_state_.host_validated &&
            (session_state_.host_capabilities.feature_flags&can_host::BurstFeatureFor(settings.mode)),
            L"Live host capability",L"Matching burst required: Classical bit12 / FD bit13");
        can_host::Configuration config;
        config.mode=settings.mode; config.nominal_bitrate_preset=settings.nominal_bitrate_preset;
        config.data_bitrate_preset=settings.data_bitrate_preset;
        const bool supported=selected && session_state_.host_validated &&
            can_host::IsConfigurationSupported(config,session_state_.host_capabilities,&error) &&
            bridge::CanPortSupports(session_state_.host_capabilities,session_state_.can_ports,settings.port,
                settings.nominal_bitrate_preset,settings.mode==can_host::Mode::CanFd);
        gate(L"Selected CAN settings",supported,error,L"Select a supported port/mode/rate and valid receive filter");
        const bool default_filter=settings.filter_selection==ui::FilterSelection::ProfileDefault;
        const bool response_filter=default_filter ||
            ((settings.filter_selection==ui::FilterSelection::CustomExtended)==settings.protocol_extended_id &&
             (settings.protocol_response_id&settings.filter_mask)==(settings.filter_id&settings.filter_mask));
        gate(L"Response filter",selected && response_filter,evidence::Hex(settings.protocol_response_id),
            L"Filter must accept the selected response ID and ID format");
        if(image) {
            const auto& i=binary_inspection_.metadata;
            const bool classical=settings.mode==can_host::Mode::Classical;
            const bool rate=selected && settings.nominal_bitrate_preset<32U && (i.can_mode_mask&(classical?1U:2U)) &&
                (i.nominal_bitrate_mask&(1UL<<settings.nominal_bitrate_preset)) &&
                (classical || (settings.data_bitrate_preset<32U && (i.data_bitrate_mask&(1UL<<settings.data_bitrate_preset))));
            gate(L"BIN / selected rate",rate,SelectedCanBitrate(),L"Use the compiled image update mode/rate");
            std::uint32_t address=0;
            gate(L"App start",SelectedApplicationStart(&address,&error) && address==i.application_start,
                evidence::Hex(address),L"Auto or exact metadata App start");
            if(live) {
                gate(L"Installed version policy",!device_version::IsBlocked(i,session_state_.target_metadata),
                    L"Installed policy vs loaded version",L"Same/newer when enabled; downgrade uses explicit maintenance action");
                const auto diff=evidence::Compare(i,session_state_.target_metadata);
                rows.insert(rows.end(),diff.begin(),diff.end());
            }
        }
    }
    MetadataDialog dialog(rows,this,evidence::PreflightTitle(rows)); dialog.DoModal();
}
