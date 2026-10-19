#include "main_frame.h"

#include <afxdlgs.h>

#include <algorithm>
#include <cwctype>
#include <cstdio>
#include <cwchar>
#include <sstream>
#include <string>

#include "build_info.generated.h"
#include "resource.h"
#include "core/operation_guidance.h"
#include "ui/log_view.h"
#include <dbt.h>

namespace {

constexpr UINT kWorkerEventMessage = WM_APP + 1U;
constexpr UINT_PTR kOperationElapsedTimerId = 1U;
constexpr UINT_PTR kComRefreshTimerId = 2U;
constexpr UINT kOperationElapsedTimerIntervalMs = 100U;
constexpr UINT kLogTextLimit = 8U * 1024U * 1024U;

std::wstring NowTimeText() {
    SYSTEMTIME time = {};
    GetLocalTime(&time);

    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%02u:%02u:%02u",
               time.wHour, time.wMinute, time.wSecond);
    return std::wstring(buffer);
}

std::wstring NowFileTimeText() {
    SYSTEMTIME time = {};
    GetLocalTime(&time);

    wchar_t buffer[48] = {};
    swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u",
               time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond);
    return std::wstring(buffer);
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int bytes = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return {};
    }

    std::string output(static_cast<std::size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, output.data(),
                        bytes - 1, nullptr, nullptr);
    return output;
}

bool ContainsCanceled(const std::wstring& text) {
    return text.find(L"canceled") != std::wstring::npos ||
           text.find(L"cancelled") != std::wstring::npos;
}

bool IsErrorLogText(const std::wstring& text) {
    std::wstring lower = text;
    std::transform(
        lower.begin(), lower.end(), lower.begin(),
        [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    const wchar_t* const error_phrases[] = {
        L"failed", L" blocked", L"timeout", L"timed out", L"invalid",
        L"mismatch", L"not accepted", L"not update-ready", L"did not",
        L"no com ports", L"bus-off", L"overflow"
    };
    for (const wchar_t* phrase : error_phrases) {
        if (lower.find(phrase) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DEVICECHANGE()
    ON_CBN_SELCHANGE(ID_I2C_SPEED_COMBO, &CMainFrame::OnI2cSettingsChanged)
    ON_EN_CHANGE(ID_I2C_ADDRESS_EDIT, &CMainFrame::OnI2cSettingsChanged)
    ON_BN_CLICKED(ID_TOP_REFRESH_COM_BTN, &CMainFrame::OnBnClickedRefreshCom)
    ON_BN_CLICKED(ID_TOP_OPEN_COM_BTN, &CMainFrame::OnBnClickedOpenCom)
    ON_BN_CLICKED(ID_TOP_CLOSE_COM_BTN, &CMainFrame::OnBnClickedCloseCom)
    ON_BN_CLICKED(ID_TOP_RESET_HOST_BTN, &CMainFrame::OnBnClickedResetHost)
    ON_BN_CLICKED(ID_CANCEL_OPERATION, &CMainFrame::OnCancelOperation)
    ON_BN_CLICKED(ID_COMPARE_METADATA, &CMainFrame::OnCompareMetadata)
    ON_BN_CLICKED(ID_PREFLIGHT, &CMainFrame::OnPreflight)
    ON_BN_CLICKED(ID_LOG_OPEN_REPORT_BTN, &CMainFrame::OnOpenLatestReport)
    ON_BN_CLICKED(ID_ERROR_DETAILS_BTN, &CMainFrame::OnErrorDetails)
    ON_BN_CLICKED(ID_FORCE_OLD_FIRMWARE, &CMainFrame::OnForceOldFirmware)
    ON_BN_CLICKED(ID_FILE_LOAD_BTN, &CMainFrame::OnBnClickedLoadBinary)
    ON_BN_CLICKED(ID_FLOW_ENTER_BOOT_BTN, &CMainFrame::OnBnClickedConnectTarget)
    ON_BN_CLICKED(ID_FLOW_EXPLICIT_ENTER_BTN,
                  &CMainFrame::OnBnClickedEnterBootloader)
    ON_BN_CLICKED(ID_FLOW_PROGRAM_BTN, &CMainFrame::OnBnClickedTransferTest)
    ON_BN_CLICKED(ID_CAN_TX_TEST_BTN, &CMainFrame::OnBnClickedCanTxTest)
    ON_BN_CLICKED(ID_FLOW_START_UPDATE_BTN,
                  &CMainFrame::OnBnClickedStartFirmwareUpdate)
    ON_BN_CLICKED(ID_FLOW_RUN_BTN, &CMainFrame::OnBnClickedDisconnect)
    ON_BN_CLICKED(ID_LOG_CLEAR_BTN, &CMainFrame::OnBnClickedClearLog)
    ON_BN_CLICKED(ID_LOG_START_BTN, &CMainFrame::OnBnClickedStartLog)
    ON_BN_CLICKED(ID_LOG_STOP_BTN, &CMainFrame::OnBnClickedStopLog)
    ON_BN_CLICKED(ID_LOG_EXPORT_SUMMARY_BTN,
                  &CMainFrame::OnBnClickedExportSummary)
    ON_BN_CLICKED(ID_LOG_DETAILED_TRACE_CHK,
                  &CMainFrame::OnBnClickedDetailedTrace)
    ON_BN_CLICKED(ID_LOG_AUTO_SCROLL_CHK, &CMainFrame::OnLogAutoScroll)
    ON_WM_PAINT()
    ON_WM_SYSCOLORCHANGE()
    ON_CBN_SELCHANGE(ID_TOP_COM_COMBO, &CMainFrame::OnCbnSelChangeCom)
    ON_CBN_SELCHANGE(ID_TOP_BAUD_COMBO, &CMainFrame::OnCbnSelChangeBaud)
    ON_CBN_SELCHANGE(ID_CAN_BITRATE_COMBO,
                     &CMainFrame::OnCbnSelChangeCanBitrate)
    ON_CBN_SELCHANGE(ID_CAN_MODE_COMBO,
                     &CMainFrame::OnCbnSelChangeCanMode)
    ON_CBN_SELCHANGE(ID_CAN_FILTER_COMBO,
                     &CMainFrame::OnCbnSelChangeCanFilter)
    ON_CBN_SELCHANGE(ID_PREFETCH_COMBO,
                     &CMainFrame::OnCbnSelChangeProfile)
    ON_EN_CHANGE(ID_APP_START_EDIT,
                 &CMainFrame::OnEnChangeApplicationStart)
    ON_MESSAGE(kWorkerEventMessage, &CMainFrame::OnWorkerEvent)
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
    ON_NOTIFY(TCN_SELCHANGE, ID_BUS_TABS, &CMainFrame::OnBusTabChanged)
    ON_CBN_SELCHANGE(ID_BUS_PORT_COMBO, &CMainFrame::OnBusPortChanged)
    ON_BN_CLICKED(ID_I2C_PROBE_BTN, &CMainFrame::OnI2cProbe)
    ON_WM_TIMER()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

CMainFrame::CMainFrame() = default;

CMainFrame::~CMainFrame() {
    worker_session_.StopAndJoin();
    StopLogRecording(false);
}

BOOL CMainFrame::PreTranslateMessage(MSG* message) {
    if (overview_tooltip_.GetSafeHwnd() != nullptr && message != nullptr) {
        overview_tooltip_.RelayEvent(message);
    }
    if (can_tx_test_tooltip_.GetSafeHwnd() != nullptr && message != nullptr) {
        can_tx_test_tooltip_.RelayEvent(message);
    }
    return CFrameWnd::PreTranslateMessage(message);
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }

    HICON icon = AfxGetApp()->LoadIcon(IDI_APP_ICON);
    cs.style = (cs.style | WS_CLIPCHILDREN | WS_CLIPSIBLINGS) & ~WS_VSCROLL;
    cs.lpszClass = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
        icon);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT create_struct) {
    if (CFrameWnd::OnCreate(create_struct) == -1) {
        return -1;
    }

    HICON icon = AfxGetApp()->LoadIcon(IDI_APP_ICON);
    SetIcon(icon, TRUE);
    SetIcon(icon, FALSE);
    ui_font_.CreatePointFont(90, L"Segoe UI");
    LOGFONT section_spec={}; ui_font_.GetLogFont(&section_spec);
    section_spec.lfWeight=FW_BOLD; section_font_.CreateFontIndirect(&section_spec);

    const auto create_or_fail =
        [this](BOOL created, const wchar_t* name) -> bool {
            if (!created) {
                AppendLog(std::wstring(L"Create control failed: ") + name);
                return false;
            }
            return true;
        };

    if (!create_or_fail(com_label_.Create(
            L"COM", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_TOP_COM_LABEL), L"COM label")) return -1;
    if (!create_or_fail(com_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_TOP_COM_COMBO), L"COM combo")) return -1;
    if (!create_or_fail(baud_label_.Create(
            L"Host Baud", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_TOP_BAUD_LABEL), L"Baud label")) return -1;
    if (!create_or_fail(baud_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_TOP_BAUD_COMBO), L"Baud combo")) return -1;
    if (!create_or_fail(refresh_com_btn_.Create(
            L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this,
            ID_TOP_REFRESH_COM_BTN), L"Refresh button")) return -1;
    if (!create_or_fail(open_com_btn_.Create(
            L"Open + Probe Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_TOP_OPEN_COM_BTN), L"Open button")) return -1;
    if (!create_or_fail(close_com_btn_.Create(
            L"Close COM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this,
            ID_TOP_CLOSE_COM_BTN), L"Close button")) return -1;
    if (!create_or_fail(reset_host_btn_.Create(
            L"Reset Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(),
            this, ID_TOP_RESET_HOST_BTN), L"Reset host button")) return -1;
    if (!create_or_fail(build_info_label_.Create(
            L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, CRect(), this,
            ID_BUILD_INFO_LABEL), L"Build info label")) return -1;
    if (!create_or_fail(cancel_btn_.Create(L"Cancel",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,
        CRect(),this,ID_CANCEL_OPERATION),L"Cancel button")) return -1;
    if (!create_or_fail(compare_metadata_btn_.Create(L"Compare Metadata",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,
        CRect(),this,ID_COMPARE_METADATA),L"Compare metadata button")) return -1;
    if (!create_or_fail(preflight_btn_.Create(L"Preflight",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,
        CRect(),this,ID_PREFLIGHT),L"Preflight button")) return -1;
    cancel_btn_.SetFont(&ui_font_); compare_metadata_btn_.SetFont(&ui_font_); preflight_btn_.SetFont(&ui_font_);
    if (!create_or_fail(force_old_firmware_btn_.Create(L"force upgrade old version firmware",
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,CRect(),this,ID_FORCE_OLD_FIRMWARE),L"Force older firmware button")) return -1;
    force_old_firmware_btn_.SetFont(&ui_font_);

    if (!create_or_fail(host_info_label_.Create(
            L"Host: not connected", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            CRect(), this, ID_HOST_INFO_LABEL),
            L"Host information label")) return -1;
    if (!create_or_fail(target_info_label_.Create(
            L"Target: not connected", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            CRect(), this, ID_TARGET_INFO_LABEL),
            L"Target information label")) return -1;
    if (!create_or_fail(target_metadata_label_.Create(
            L"Device metadata: not available",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            CRect(), this, ID_TARGET_METADATA_INFO),
            L"Target metadata label")) return -1;

    if (!create_or_fail(can_mode_label_.Create(
            L"CAN Mode", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_CAN_MODE_LABEL), L"CAN mode label")) return -1;
    if (!create_or_fail(can_mode_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_CAN_MODE_COMBO), L"CAN mode combo")) return -1;
    if (!create_or_fail(can_bitrate_label_.Create(
            L"Nominal", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_CAN_BITRATE_LABEL), L"CAN bitrate label")) return -1;
    if (!create_or_fail(can_bitrate_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_CAN_BITRATE_COMBO),
            L"CAN bitrate combo")) return -1;
    if (!create_or_fail(can_data_label_.Create(
            L"Data", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_CAN_DATA_LABEL), L"CAN data bitrate label")) return -1;
    if (!create_or_fail(can_data_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_CAN_DATA_COMBO),
            L"CAN data bitrate combo")) return -1;
    if (!create_or_fail(can_tx_test_btn_.Create(
            L"Start Bridge Board Self Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_CAN_TX_TEST_BTN),
            L"Bridge board CAN TX self-test button")) return -1;
    if (!create_or_fail(can_tx_test_help_.Create(
            L"?", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE |
                SS_NOTIFY | WS_BORDER,
            CRect(), this, ID_CAN_TX_TEST_HELP),
            L"Bridge board CAN TX self-test help")) return -1;
    if (!create_or_fail(can_tx_test_tooltip_.Create(this, TTS_ALWAYSTIP),
            L"Bridge board CAN TX self-test tooltip")) return -1;
    can_tx_test_tooltip_.AddTool(
        &can_tx_test_help_,
        L"Use a CAN analyzer to observe the physical CAN bus.\r\n"
        L"Expected every 1000 ms: standard ID 0x123, DLC 8,\r\n"
        L"data 11 22 33 44 55 66 77 XX; XX starts at 00 and increments.");
    can_tx_test_tooltip_.SetMaxTipWidth(480);
    can_tx_test_tooltip_.AddTool(&baud_combo_,
        L"PC <-> MCU host Host Link only. Power-on115200; APP negotiates460800/921600. "
        L"Independent of CAN/I2C and future device UART baud fixed by BIN metadata. BOOT self-update uses its fixed rate.");
    can_tx_test_tooltip_.Activate(TRUE);
    if (!create_or_fail(can_filter_label_.Create(
            L"RX Filter", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_CAN_FILTER_LABEL), L"CAN filter label")) return -1;
    if (!create_or_fail(can_filter_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_CAN_FILTER_COMBO), L"CAN filter combo")) return -1;
    if (!create_or_fail(can_filter_id_label_.Create(
            L"ID", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_CAN_FILTER_ID_LABEL), L"CAN filter ID label")) return -1;
    if (!create_or_fail(can_filter_id_edit_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            CRect(), this, ID_CAN_FILTER_ID_EDIT),
            L"CAN filter ID edit")) return -1;
    if (!create_or_fail(can_filter_mask_label_.Create(
            L"Mask", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_CAN_FILTER_MASK_LABEL), L"CAN filter mask label")) return -1;
    if (!create_or_fail(can_filter_mask_edit_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            CRect(), this, ID_CAN_FILTER_MASK_EDIT),
            L"CAN filter mask edit")) return -1;
    if (!create_or_fail(profile_label_.Create(
            L"Workflow", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_PREFETCH_LABEL), L"Workflow label")) return -1;
    if (!create_or_fail(profile_combo_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST,
            CRect(), this, ID_PREFETCH_COMBO), L"Profile combo")) return -1;

    if (!create_or_fail(connect_target_btn_.Create(
            L"Connect Target", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_FLOW_ENTER_BOOT_BTN),
            L"Connect target button")) return -1;
    if (!create_or_fail(enter_bootloader_btn_.Create(
            L"Enter Bootloader", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_FLOW_EXPLICIT_ENTER_BTN),
            L"Enter bootloader button")) return -1;
    if (!create_or_fail(transfer_test_btn_.Create(
            L"Transfer Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_FLOW_PROGRAM_BTN),
            L"Transfer test button")) return -1;
    if (!create_or_fail(disconnect_btn_.Create(
            L"Disconnect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_FLOW_RUN_BTN), L"Disconnect button")) return -1;
    if (!create_or_fail(operation_timer_label_.Create(
            L"Elapsed: 00:00.0", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_PROGRAM_TIMER_LABEL), L"Operation timer label")) return -1;

    if (!create_or_fail(start_firmware_update_btn_.Create(
            L"Start Firmware Update",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_FLOW_START_UPDATE_BTN),
            L"Start firmware update button")) return -1;
    if (!create_or_fail(firmware_prerequisites_label_.Create(
            L"Firmware update prerequisites are not ready.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            CRect(), this, ID_UPDATE_PREREQUISITES),
            L"Firmware update prerequisites label")) return -1;

    if (!create_or_fail(load_binary_btn_.Create(
            L"Load Update Image", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_FILE_LOAD_BTN),
            L"Load firmware button")) return -1;
    if (!create_or_fail(file_path_label_.Create(
            L"Update image: (none)", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            CRect(), this, ID_FILE_PATH_LABEL),
            L"Firmware path label")) return -1;
    if (!create_or_fail(app_start_label_.Create(
            L"App Start Override", WS_CHILD | WS_VISIBLE, CRect(), this,
            ID_APP_START_LABEL), L"Application start label")) return -1;
    if (!create_or_fail(app_start_edit_.Create(
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            CRect(), this, ID_APP_START_EDIT),
            L"Application start edit")) return -1;
    const wchar_t* firmware_field_captions[kFirmwareFieldCount] = {
        L"BIN Size", L"Calculated CRC32", L"Stored CRC32",
        L"CRC Result", L"Metadata Status", L"MCU",
        L"App Version", L"Build", L"Target", L"Protocol / Format",
        L"App Range", L"Image / Capacity", L"Flash / Boot",
        L"Minimum / Erase", L"Interface / Rate", L"Routing / IDs",
        L"Update Policy", L"Boot / IRQ ABI", L"BIN Version Policy", L"Installed Policy"
    };
    for (std::size_t index = 0U; index < kFirmwareFieldCount; ++index) {
        if (!create_or_fail(firmware_field_labels_[index].Create(
                firmware_field_captions[index],
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                CRect(), this,
                ID_FIRMWARE_FIELD_LABEL_BASE + static_cast<UINT>(index)),
                L"Firmware field label")) return -1;
        if (!create_or_fail(firmware_field_values_[index].Create(
                L"N/A",
                WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT |
                    SS_CENTERIMAGE | SS_PATHELLIPSIS | SS_NOPREFIX,
                CRect(), this,
                ID_FIRMWARE_FIELD_VALUE_BASE + static_cast<UINT>(index)),
                L"Firmware field value")) return -1;
    }

    if (!create_or_fail(progress_bar_.Create(
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(), this,
            ID_PROGRESS_BAR), L"Progress bar")) return -1;
    if (!create_or_fail(progress_label_.Create(
            L"Progress: 0%", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP |
                SS_CENTERIMAGE | SS_NOPREFIX, CRect(), this,
            ID_PROGRESS_LABEL), L"Progress label")) return -1;

    if (!create_or_fail(log_edit_.Create(
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            CRect(), this, ID_LOG_EDIT), L"Log edit")) return -1;
    if (!create_or_fail(clear_log_btn_.Create(
            L"Clear Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_LOG_CLEAR_BTN), L"Clear log button")) return -1;
    if (!create_or_fail(start_log_btn_.Create(
            L"Start Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_LOG_START_BTN), L"Start log button")) return -1;
    if (!create_or_fail(stop_log_btn_.Create(
            L"Stop Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_LOG_STOP_BTN), L"Stop log button")) return -1;
    if (!create_or_fail(export_summary_btn_.Create(
            L"Export Summary", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(), this, ID_LOG_EXPORT_SUMMARY_BTN),
            L"Export summary button")) return -1;
    if (!create_or_fail(detailed_trace_check_.Create(
            L"Detailed trace (slower)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            CRect(), this, ID_LOG_DETAILED_TRACE_CHK),
            L"Detailed trace checkbox")) return -1;
    if (!create_or_fail(open_report_btn_.Create(L"Open Last Report",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        CRect(),this,ID_LOG_OPEN_REPORT_BTN),L"Open report button")) return -1;
    if (!create_or_fail(error_details_btn_.Create(L"Error Details",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        CRect(),this,ID_ERROR_DETAILS_BTN),L"Error details button")) return -1;
    if (!create_or_fail(log_auto_scroll_check_.Create(L"Auto scroll down",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,CRect(),this,ID_LOG_AUTO_SCROLL_CHK),
        L"Log auto scroll checkbox")) return -1;
    log_auto_scroll_check_.SetCheck(BST_CHECKED);

    if (!InitializeBusUi()) return -1;

    CWnd* font_controls[] = {
        &com_label_, &com_combo_, &baud_label_, &baud_combo_,
        &refresh_com_btn_, &open_com_btn_, &close_com_btn_, &reset_host_btn_,
        &build_info_label_, &host_info_label_, &target_info_label_,
        &target_metadata_label_,
        &can_mode_label_, &can_mode_combo_, &can_bitrate_label_,
        &can_bitrate_combo_, &can_data_label_, &can_data_combo_,
        &can_tx_test_btn_, &can_tx_test_help_,
        &can_filter_label_, &can_filter_combo_, &can_filter_id_label_,
        &can_filter_id_edit_, &can_filter_mask_label_,
        &can_filter_mask_edit_, &profile_label_,
        &profile_combo_, &connect_target_btn_, &enter_bootloader_btn_,
        &transfer_test_btn_, &disconnect_btn_, &operation_timer_label_,
        &start_firmware_update_btn_, &firmware_prerequisites_label_,
        &load_binary_btn_, &file_path_label_, &app_start_label_,
        &app_start_edit_, &progress_label_,
        &log_edit_, &clear_log_btn_,
        &start_log_btn_, &stop_log_btn_, &export_summary_btn_,
        &detailed_trace_check_, &log_auto_scroll_check_, &open_report_btn_, &error_details_btn_,
    };
    for (CWnd* control : font_controls) {
        control->SetFont(&ui_font_);
    }
    for (std::size_t index = 0U; index < kFirmwareFieldCount; ++index) {
        firmware_field_labels_[index].SetFont(&ui_font_);
        firmware_field_values_[index].SetFont(&ui_font_);
    }

    baud_combo_.AddString(L"115200");
    baud_combo_.AddString(L"460800");
    baud_combo_.AddString(L"921600");
    baud_combo_.SetCurSel(0);

    can_mode_combo_.AddString(L"Classical CAN");
    can_mode_combo_.AddString(L"CAN FD");
    can_mode_combo_.SetCurSel(0);
    can_bitrate_combo_.AddString(L"500K");
    can_bitrate_combo_.AddString(L"250K");
    can_bitrate_combo_.AddString(L"1M");
    can_bitrate_combo_.SetCurSel(0);
    can_data_combo_.AddString(L"N/A");
    can_data_combo_.AddString(L"2M");
    can_data_combo_.SetCurSel(0);
    can_filter_combo_.AddString(L"Metadata/default");
    can_filter_combo_.AddString(L"Custom standard");
    can_filter_combo_.AddString(L"Custom extended");
    can_filter_combo_.SetCurSel(0);
    can_filter_id_edit_.SetWindowTextW(L"0x701");
    can_filter_mask_edit_.SetWindowTextW(L"0x7FF");
    profile_combo_.AddString(L"Custom Transfer");
    profile_combo_.AddString(L"CAN Upgrade");
    profile_combo_.SetCurSel(0);
    app_start_edit_.SetLimitText(10U);
    app_start_edit_.SetWindowTextW(L"Auto");

    progress_bar_.SetRange32(0, 100);
    progress_bar_.SetPos(0);
    log_edit_.LimitText(kLogTextLimit);
    log_edit_.SetBackgroundColor(FALSE,GetSysColor(COLOR_WINDOW));
    build_info_label_.SetWindowTextW(
        (std::wstring(L"Build ") + FIRMWARE_UPGRADE_TOOL_VERSION + L"  " +
         FIRMWARE_UPGRADE_TOOL_BUILD_TIME).c_str());

    InitializeSettingsFile();
    LoadUiSettings();
    RefreshComPorts();

    worker_started_ = worker_session_.Start(m_hWnd, kWorkerEventMessage);
    if (!worker_started_) {
        AppendLog(L"Failed to start the communication worker.");
        return -1;
    }
    worker_session_.SetDetailedTrace(
        detailed_trace_check_.GetCheck() == BST_CHECKED);

    UpdateUi();
    AppendLog(L"Firmware Upgrade Tool initialized.");
    AppendLog(L"Host Link v2 is required before CAN controls are enabled.");

    CRect client_rect;
    GetClientRect(&client_rect);
    LayoutControls(client_rect.Width(), client_rect.Height());
    return 0;
}

void CMainFrame::OnSize(UINT type, int width, int height) {
    CFrameWnd::OnSize(type, width, height);
    LayoutControls(width, height);
}

void CMainFrame::AppendLog(const std::wstring& text) {
    if (log_edit_.GetSafeHwnd() == nullptr) {
        return;
    }

    const std::wstring line = L"[" + NowTimeText() + L"] " + text +
                              L"\r\n";
    firmware_upgrade::log_view::Append(log_edit_.GetSafeHwnd(),line,
        IsErrorLogText(text) ? RGB(196,24,24) : GetSysColor(COLOR_WINDOWTEXT),log_auto_scroll_);

    if (log_recording_ && log_record_fp_ != nullptr) {
        const std::string utf8 = WideToUtf8(line);
        if (!utf8.empty()) {
            fwrite(utf8.data(), 1U, utf8.size(), log_record_fp_);
            fflush(log_record_fp_);
        }
    }
}

std::wstring CMainFrame::BuildSessionSummary() const {
    if (IsMiscTab()) return BuildBridgeSummary();
    std::wstringstream stream;
    CString mode;
    CString nominal;
    CString data;
    CString filter;
    CString filter_id;
    CString filter_mask;
    CString application_start;
    can_mode_combo_.GetWindowTextW(mode);
    can_bitrate_combo_.GetWindowTextW(nominal);
    can_data_combo_.GetWindowTextW(data);
    can_filter_combo_.GetWindowTextW(filter);
    can_filter_id_edit_.GetWindowTextW(filter_id);
    can_filter_mask_edit_.GetWindowTextW(filter_mask);
    app_start_edit_.GetWindowTextW(application_start);

    stream << L"Firmware Upgrade Tool session summary\r\n"
           << L"Generated: " << NowTimeText() << L"\r\n"
           << L"Tool: " << FIRMWARE_UPGRADE_TOOL_VERSION << L" ("
           << FIRMWARE_UPGRADE_TOOL_BUILD_TIME << L")\r\n"
           << BuildI2cSummary()
           << L"Host: " << (summary_host_.empty() ? L"not validated" :
                                                summary_host_) << L"\r\n"
           << L"Target: " << (summary_target_.empty() ? L"not validated" :
                                                    summary_target_) << L"\r\n"
           << L"CAN active configuration: "
           << (summary_can_.empty() ? L"not started" : summary_can_)
           << L"\r\n"
           << L"UI selection: " << mode.GetString() << L", nominal "
           << nominal.GetString() << L", data " << data.GetString()
           << L", filter " << filter.GetString() << L" "
           << filter_id.GetString() << L"/" << filter_mask.GetString()
           << L", app start " << application_start.GetString() << L"\r\n"
           << L"Image: " << (summary_image_.empty() ? L"not used" :
                                                  summary_image_) << L"\r\n"
           << L"Image file: " << (binary_path_.empty() ? L"not loaded" :
                                                   binary_path_) << L"\r\n"
           << L"Host Link counters: requests=" << summary_host_requests_
           << L", retries=" << summary_host_retries_
           << L", timeouts=" << summary_host_timeouts_
           << L", unmatched=" << summary_unmatched_frames_
           << L", events=" << summary_received_events_
           << L", dropped_events=" << summary_dropped_events_ << L"\r\n"
           << L"Arbitration lost events: "
           << summary_arbitration_lost_entries_ << L"\r\n"
           << L"F24 counters: ";
    if (summary_f24_stats_valid_) {
        stream << L"UART RX/TX=" << summary_f24_stats_.uart_rx_bytes << L"/"
               << summary_f24_stats_.uart_tx_bytes
               << L", UART CRC/invalid/overflow="
               << summary_f24_stats_.uart_crc_errors << L"/"
               << summary_f24_stats_.uart_invalid_errors << L"/"
               << summary_f24_stats_.uart_rx_overflows
               << L", CAN RX/TX/lost/fail="
               << summary_f24_stats_.can_rx_frames << L"/"
               << summary_f24_stats_.can_tx_frames << L"/"
               << summary_f24_stats_.can_rx_lost_frames << L"/"
               << summary_f24_stats_.can_tx_failures
               << L", warning/passive/bus-off/recovery="
               << summary_f24_stats_.can_warning_entries << L"/"
               << summary_f24_stats_.can_error_passive_entries << L"/"
               << summary_f24_stats_.can_bus_off_entries << L"/"
               << summary_f24_stats_.can_recovery_count
               << L", host_command_errors="
               << summary_f24_stats_.host_command_errors << L"\r\n";
    } else {
        stream << L"not read\r\n";
    }
    stream
           << L"Elapsed: " << summary_elapsed_ms_ << L" ms\r\n"
           << L"Final result: " << summary_result_ << L"\r\n";
    return stream.str();
}

void CMainFrame::ExportSessionSummary() {
    const std::wstring default_name =
        L"firmware_upgrade_summary_" + NowFileTimeText() + L".txt";
    CFileDialog dialog(
        FALSE, L"txt", default_name.c_str(),
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        L"Text Files (*.txt)|*.txt|All Files (*.*)|*.*||", this);
    if (dialog.DoModal() != IDOK) {
        return;
    }

    FILE* file = nullptr;
    const std::wstring path = dialog.GetPathName().GetString();
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr) {
        MessageBoxW(L"Failed to create session summary.", L"Export Summary",
                    MB_ICONERROR | MB_OK);
        return;
    }
    const unsigned char bom[3] = {0xEFU, 0xBBU, 0xBFU};
    const std::string utf8 = WideToUtf8(BuildSessionSummary());
    fwrite(bom, 1U, sizeof(bom), file);
    if (!utf8.empty()) {
        fwrite(utf8.data(), 1U, utf8.size(), file);
    }
    fclose(file);
    AppendLog(L"Session summary exported: " + path);
}

void CMainFrame::StartLogRecording() {
    if (log_recording_) {
        return;
    }
    if (ini_path_.empty()) {
        InitializeSettingsFile();
    }

    std::wstring directory = ini_path_;
    const std::size_t position = directory.find_last_of(L"\\/");
    if (position != std::wstring::npos) {
        directory.resize(position);
    } else {
        directory = L".";
    }

    log_record_path_ =
        directory + L"\\firmware_upgrade_tool_" + NowFileTimeText() + L".log";
    FILE* file = nullptr;
    if (_wfopen_s(&file, log_record_path_.c_str(), L"wb") != 0 ||
        file == nullptr) {
        MessageBoxW(
            L"Failed to create log file.", L"Start Log",
            MB_ICONERROR | MB_OK);
        return;
    }

    const unsigned char bom[3] = {0xEFU, 0xBBU, 0xBFU};
    fwrite(bom, 1U, sizeof(bom), file);
    log_record_fp_ = file;
    log_recording_ = true;
    UpdateUi();
    AppendLog(
        std::wstring(L"Log recording started. Tool ") +
        FIRMWARE_UPGRADE_TOOL_VERSION + L", build " +
        FIRMWARE_UPGRADE_TOOL_BUILD_TIME + L".");
    AppendLog(L"Log file: " + log_record_path_);
    if (!session_state_.host_summary.empty()) {
        AppendLog(L"Session host: " + session_state_.host_summary);
    }
    if (!session_state_.target_summary.empty()) {
        AppendLog(L"Session target: " + session_state_.target_summary);
    }
}

void CMainFrame::StopLogRecording(bool emit_log) {
    if (!log_recording_) {
        return;
    }

    const std::wstring path = log_record_path_;
    if (log_record_fp_ != nullptr) {
        fflush(log_record_fp_);
        fclose(log_record_fp_);
    }
    log_record_fp_ = nullptr;
    log_recording_ = false;
    log_record_path_.clear();
    UpdateUi();
    if (emit_log) {
        AppendLog(L"Log recording stopped: " + path);
    }
}

void CMainFrame::RefreshComPorts() {
    const std::wstring previous = SelectedComPort();
    com_combo_.ResetContent();
    com_ports_ = firmware_upgrade::serial_ports::Enumerate();
    const std::wstring preferred = previous.empty() ?
        preferred_com_port_ : previous;
    com_device_notice_=firmware_upgrade::serial_ports::DescribePreviousDevice(com_ports_,
        verified_com_port_.empty()?preferred:verified_com_port_,selected_com_instance_);
    if(!com_device_notice_.empty()) AppendLog(com_device_notice_);
    int selected = -1;
    for (std::size_t index = 0U; index < com_ports_.size(); ++index) {
        const int item = com_combo_.AddString(com_ports_[index].DisplayName().c_str());
        if (item >= 0) {
            com_combo_.SetItemData(item, static_cast<DWORD_PTR>(index));
            if (com_ports_[index].port_name == preferred) {
                selected = item;
            }
        }
    }

    CRect client;
    GetClientRect(&client);
    LayoutControls(client.Width(), client.Height());
    if (com_combo_.GetCount() == 0) {
        AppendLog(L"No COM ports found.");
        return;
    }

    com_combo_.SetCurSel(selected<0 && preferred.empty()?0:selected);
    preferred_com_port_=preferred;
    SaveUiSettings();
    AppendLog(L"COM scan completed.");
}

std::wstring CMainFrame::SelectedComPort() const {
    if (com_combo_.GetSafeHwnd() == nullptr) {
        return {};
    }
    const int item = com_combo_.GetCurSel();
    if (item == CB_ERR) {
        return {};
    }
    const DWORD_PTR index = com_combo_.GetItemData(item);
    return index < com_ports_.size() ? com_ports_[index].port_name : L"";
}

DWORD CMainFrame::SelectedBaudrate() const {
    CString text;
    baud_combo_.GetWindowTextW(text);
    const std::wstring value = text.GetString();
    if (value == L"460800") {
        return 460800U;
    }
    if (value == L"921600") {
        return 921600U;
    }
    return 115200U;
}

void CMainFrame::SetProgress(int value, const std::wstring& label) {
    const int clamped = (std::max)(0, (std::min)(value, 100));
    progress_bar_.SetPos(clamped);
    progress_label_.SetWindowTextW(
        (label + L": " + std::to_wstring(clamped) + L"%").c_str());
    if (progress_operation_ != label) {
        progress_operation_ = label;
        CRect client;
        GetClientRect(&client);
        LayoutControls(client.Width(), client.Height());
    }
}

void CMainFrame::ApplyWorkerEvent(
    const firmware_upgrade::ui::WorkerEvent& event) {
    using firmware_upgrade::ui::WorkerEventKind;

    switch (event.kind) {
        case WorkerEventKind::Log:
            AppendLog(event.text);
            break;
        case WorkerEventKind::State:
            session_state_ = event.state;
            if (event.state.busy)
                last_update_result_=firmware_upgrade::feedback::Result::None;
            if(event.state.host_validated && !last_probe_port_.empty() && event.state.port_name==last_probe_port_) {
                selected_com_instance_=pending_com_instance_;
                verified_com_port_=last_probe_port_;
                last_probe_port_.clear(); pending_com_instance_.clear(); com_device_notice_.clear(); SaveUiSettings();
            }
            if (!event.state.host_summary.empty()) {
                summary_host_ = event.state.host_summary;
            }
            if (!event.state.target_summary.empty()) {
                summary_target_ = event.state.target_summary;
            }
            if (!event.state.can_summary.empty()) {
                summary_can_ = event.state.can_summary;
            }
            if (!event.state.image_summary.empty()) {
                summary_image_ = event.state.image_summary;
            }
            summary_host_requests_ = event.state.host_requests;
            summary_host_retries_ = event.state.host_retries;
            summary_host_timeouts_ = event.state.host_timeouts;
            summary_unmatched_frames_ = event.state.unmatched_frames;
            summary_received_events_ = event.state.received_events;
            summary_dropped_events_ = event.state.dropped_events;
            summary_arbitration_lost_entries_ =
                event.state.arbitration_lost_entries;
            if (event.state.f24_stats_valid) {
                summary_f24_stats_ = event.state.f24_stats;
                summary_f24_stats_valid_ = true;
            }
            worker_command_pending_ = event.state.busy;
            UpdateUi();
            break;
        case WorkerEventKind::Progress:
            SetProgress(event.progress, event.text);
            break;
        case WorkerEventKind::UpdateReport:
            SaveUpdateReport(event);
            break;
        case WorkerEventKind::OperationComplete:
            worker_command_pending_ = false;
            last_update_result_=event.update_result;
            last_operation_error_ = event.success ? L"" : event.text;
            last_error_guidance_=event.success?L"":event.guidance;
            summary_elapsed_ms_ = operation_timer_running_ ?
                GetTickCount64() - operation_timer_start_ms_ : 0U;
            summary_result_ = event.success ?
                L"PASS: " + event.text : L"FAIL: " + event.text;
            StopOperationElapsedTimer();
            if (event.success) {
                AppendLog(L"Operation completed successfully: " + event.text);
            } else {
                AppendLog(L"Operation failed: " + event.text);
                const auto advice=firmware_upgrade::guidance::Explain(event.text);
                AppendLog(L"Suggested checks ["+advice.layer+L"]: "+advice.next);
                if (!closing_ && !ContainsCanceled(event.text)) {
                    MessageBoxW(
                        (event.guidance.empty()?firmware_upgrade::guidance::Format(event.text):event.guidance).c_str(), L"Operation Error / Suggested Checks",
                        MB_ICONERROR | MB_OK);
                }
            }
            UpdateUi();
            if (event.success && event.update_result!=firmware_upgrade::feedback::Result::None && !closing_) {
                const auto feedback=firmware_upgrade::feedback::Describe(event.update_result);
                SetProgress(100,feedback.progress);
                MessageBoxW((feedback.status+L"\r\n\r\n"+feedback.next).c_str(),feedback.title.c_str(),
                    MB_OK | (feedback.warning ? MB_ICONWARNING : MB_ICONINFORMATION));
            }
            break;
    }
}

void CMainFrame::StartOperationElapsedTimer() {
    last_update_result_=firmware_upgrade::feedback::Result::None;
    operation_timer_start_ms_ = GetTickCount64();
    operation_timer_running_ = true;
    SetTimer(
        kOperationElapsedTimerId, kOperationElapsedTimerIntervalMs, nullptr);
    UpdateOperationElapsedText();
}

void CMainFrame::StopOperationElapsedTimer() {
    if (!operation_timer_running_) {
        return;
    }
    KillTimer(kOperationElapsedTimerId);
    operation_timer_running_ = false;
    UpdateOperationElapsedText();
}

void CMainFrame::UpdateOperationElapsedText() {
    ULONGLONG elapsed_ms = 0U;
    if (operation_timer_running_) {
        elapsed_ms = GetTickCount64() - operation_timer_start_ms_;
    }

    const ULONGLONG total_tenths = elapsed_ms / 100U;
    const ULONGLONG tenths = total_tenths % 10U;
    const ULONGLONG total_seconds = total_tenths / 10U;
    const ULONGLONG seconds = total_seconds % 60U;
    const ULONGLONG minutes = total_seconds / 60U;

    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"Elapsed: %02llu:%02llu.%01llu",
               minutes, seconds, tenths);
    operation_timer_label_.SetWindowTextW(buffer);
}

void CMainFrame::OnBnClickedRefreshCom() {
    RefreshComPorts();
}

void CMainFrame::OnBnClickedOpenCom() {
    QueueOpenSelectedCom();
}

void CMainFrame::OnBnClickedCloseCom() {
    QueueDisconnect();
}

void CMainFrame::OnBnClickedResetHost() {
    if (!session_state_.host_validated) {
        AppendLog(L"Reset blocked: MCU host is not validated.");
        return;
    }
    if (MessageBoxW(
            L"Reset the connected MCU bridge now?\n\n"
            L"The tool will stop CAN, send RESET_HOST once, close the current "
            L"session, and automatically reconnect and probe the same COM "
            L"port.",
            L"Reset MCU Host", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) !=
        IDYES) {
        return;
    }
    if (!worker_session_.ResetHost()) {
        AppendLog(L"Reset blocked: another worker operation is active.");
        return;
    }
    worker_command_pending_ = true;
    AppendLog(L"Queued bridge reset and bounded reconnect.");
    UpdateUi();
}

void CMainFrame::OnBnClickedClearLog() {
    log_edit_.SetWindowTextW(L"");
}

void CMainFrame::OnBnClickedStartLog() {
    StartLogRecording();
}

void CMainFrame::OnBnClickedStopLog() {
    StopLogRecording(true);
}

void CMainFrame::OnBnClickedExportSummary() {
    ExportSessionSummary();
}

void CMainFrame::OnBnClickedDetailedTrace() {
    const bool enabled =
        detailed_trace_check_.GetCheck() == BST_CHECKED;
    worker_session_.SetDetailedTrace(enabled);
    SaveUiSettings();
    AppendLog(enabled ?
        L"Detailed UART/CAN trace enabled; full-image updates may run slower." :
        L"Detailed UART/CAN trace disabled.");
}

void CMainFrame::OnCbnSelChangeCom() {
    com_device_notice_.clear();
    SaveUiSettings();
    UpdateUi();
}

void CMainFrame::OnCbnSelChangeBaud() {
    SaveUiSettings();
}

void CMainFrame::OnCbnSelChangeCanBitrate() {
    SaveUiSettings();
    QueueCanConfiguration();
    UpdateUi();
}

void CMainFrame::OnCbnSelChangeCanMode() {
    CString text;
    can_mode_combo_.GetWindowTextW(text);
    const bool can_fd = std::wstring(text.GetString()) == L"CAN FD";
    can_data_combo_.SetCurSel(can_fd ? 1 : 0);
    SaveUiSettings();
    QueueCanConfiguration();
    UpdateUi();
}

void CMainFrame::OnCbnSelChangeCanFilter() {
    SaveUiSettings();
    UpdateUi();
}

void CMainFrame::OnCbnSelChangeProfile() {
    if (SelectedProfile() == L"Custom Transfer") {
        can_filter_id_edit_.SetWindowTextW(L"0x711");
    } else {
        can_filter_id_edit_.SetWindowTextW(L"0x701");
    }
    can_filter_mask_edit_.SetWindowTextW(L"0x7FF");
    if (!binary_data_.empty()) {
        ValidateLoadedFirmwareImage(true);
    }
    SaveUiSettings();
    UpdateUi();
}

void CMainFrame::OnEnChangeApplicationStart() {
    if (loading_ui_settings_) return;
    if (!binary_data_.empty()) {
        ValidateLoadedFirmwareImage(false);
    }
    UpdateUi();
}

void CMainFrame::OnTimer(UINT_PTR timer_id) {
    if(timer_id==kComRefreshTimerId) {
        if(worker_command_pending_ || session_state_.busy || session_state_.serial_open) return;
        KillTimer(kComRefreshTimerId); RefreshComPorts(); UpdateUi(); return;
    }
    if (timer_id == kOperationElapsedTimerId &&
        operation_timer_running_) {
        UpdateOperationElapsedText();
        return;
    }
    CFrameWnd::OnTimer(timer_id);
}
BOOL CMainFrame::OnDeviceChange(UINT event_type,DWORD_PTR data) {
    if(event_type==DBT_DEVNODES_CHANGED || event_type==DBT_DEVICEARRIVAL || event_type==DBT_DEVICEREMOVECOMPLETE) {
        // Debounce inventory refresh; never open/probe, close COM or change a worker session here.
        SetTimer(kComRefreshTimerId,750U,nullptr);
        if(session_state_.serial_open || session_state_.busy || worker_command_pending_)
            AppendLog(L"Windows device inventory changed; COM refresh deferred until the worker is idle and disconnected. No automatic probe or rewrite.");
    }
    return CFrameWnd::OnDeviceChange(event_type,data);
}

LRESULT CMainFrame::OnWorkerEvent(WPARAM, LPARAM) {
    const std::vector<firmware_upgrade::ui::WorkerEvent> events =
        worker_session_.DrainEvents();
    for (const firmware_upgrade::ui::WorkerEvent& event : events) {
        ApplyWorkerEvent(event);
    }
    return 0;
}

void CMainFrame::OnClose() {
    closing_ = true;
    SaveUiSettings();
    StopOperationElapsedTimer();
    worker_session_.StopAndJoin();
    StopLogRecording(false);
    CFrameWnd::OnClose();
}
