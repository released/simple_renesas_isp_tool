#include "main_frame.h"
#include "resource.h"
#include "build_info.generated.h"
#include "core/update_evidence.h"
#include "core/update_report_store.h"
#include "core/device_version_policy.h"
#include <afxdlgs.h>

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
    MetadataDialog(const std::vector<firmware_upgrade::evidence::Difference>& rows,CWnd* parent)
        : CDialog(IDD_METADATA_DIFFERENCE,parent), rows_(rows) {}
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog(); list_.SubclassDlgItem(ID_METADATA_DIFFERENCE_LIST,this);
        list_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
        CRect r; list_.GetClientRect(&r);
        list_.InsertColumn(0,L"Field",LVCFMT_LEFT,r.Width()*26/100);
        list_.InsertColumn(1,L"Loaded image",LVCFMT_LEFT,r.Width()*27/100);
        list_.InsertColumn(2,L"Live device",LVCFMT_LEFT,r.Width()*27/100);
        list_.InsertColumn(3,L"Check",LVCFMT_LEFT,r.Width()*18/100);
        for(const auto& row:rows_) {
            const int i=list_.InsertItem(list_.GetItemCount(),row.field.c_str());
            list_.SetItemText(i,1,row.image.c_str());list_.SetItemText(i,2,row.live.c_str());
            list_.SetItemText(i,3,row.result.c_str());
        }
        CenterWindow(); return TRUE;
    }
    BOOL OnNotify(WPARAM w,LPARAM l,LRESULT* result) override {
        const auto* header=reinterpret_cast<const NMHDR*>(l);
        if(header && header->hwndFrom==list_.GetSafeHwnd() && header->code==NM_CUSTOMDRAW) {
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
};
}
bool CMainFrame::ConfirmForceOldFirmware() {
    ForcePasswordDialog dialog(this); return dialog.DoModal()==IDOK;
}
void CMainFrame::OnForceOldFirmware() {
    if(IsMiscTab() || IsI2cTab()) return;
    StartDeviceUpdate(true);
}
void CMainFrame::OnCancelOperation() {
    if(worker_session_.RequestCancel()) AppendLog(L"Cancel requested. Worker will attempt ABORT/stop cleanup; COM remains owned by the worker.");
}
void CMainFrame::OnCompareMetadata() {
    std::vector<firmware_upgrade::evidence::Difference> rows;
    if(IsMiscTab() && binary_bridge_inspection_.valid && session_state_.bridge_info_valid)
        rows=firmware_upgrade::evidence::Compare(binary_bridge_inspection_.metadata,session_state_.bridge_info);
    else if(!IsI2cTab() && !IsMiscTab() && binary_inspection_.metadata_valid && session_state_.target_validated)
        rows=firmware_upgrade::evidence::Compare(binary_inspection_.metadata,session_state_.target_metadata);
    if(rows.empty()) {AppendLog(L"Compare Metadata requires a valid image and live metadata for the selected destination.");return;}
    MetadataDialog dialog(rows,this); dialog.DoModal();
}
void CMainFrame::SaveUpdateReport(const std::wstring& json) {
    wchar_t exe[MAX_PATH]={}; const DWORD path_size=GetModuleFileNameW(nullptr,exe,MAX_PATH);
    if(!path_size || path_size>=MAX_PATH) {AppendLog(L"Automatic report path resolution failed; use Export Summary.");return;}
    std::wstring directory=exe; directory=directory.substr(0,directory.find_last_of(L"\\/"))+L"\\reports";
    if(!CreateDirectoryW(directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) {
        AppendLog(L"Automatic report directory failed: "+std::to_wstring(GetLastError()));return;
    }
    SYSTEMTIME t={};GetLocalTime(&t);
    wchar_t name[100]={};swprintf_s(name,L"\\update_%04u%02u%02u_%02u%02u%02u_%03u_%llu.json",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetTickCount64());
    const std::wstring path=directory+name;
    SYSTEMTIME utc={};GetSystemTime(&utc);
    wchar_t stamp[40]={};swprintf_s(stamp,L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
        utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds);
    const auto document=L"{\n  \"tool\": "+firmware_upgrade::evidence::JsonString(FIRMWARE_UPGRADE_TOOL_VERSION)+
        L",\n  \"build\": "+firmware_upgrade::evidence::JsonString(FIRMWARE_UPGRADE_TOOL_BUILD_TIME)+
        L",\n  \"recorded_at_utc\": "+firmware_upgrade::evidence::JsonString(stamp)+L",\n  \"update\": "+json+L"}\n";
    std::wstring error;
    const bool ok=firmware_upgrade::evidence::WriteReport(path,document,&error);
    AppendLog(ok?L"Automatic update report: "+path:L"Automatic report write failed: "+path+L"; "+error);
}
