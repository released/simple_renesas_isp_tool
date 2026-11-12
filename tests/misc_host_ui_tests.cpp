// Actual MFC controls with synthetic live descriptors; never opens COM or Flash.
#include <afxwin.h>
#include <afxcmn.h>
#include <atlimage.h>
#include <cstring>
#include <iostream>
#include "../src/main_frame.h"
#include "../src/resource.h"

CWinApp test_app;
int TestSerialPortCatalog();
namespace {
unsigned cases = 0U, failures = 0U;
bool preflight_checked = false, expected_boot = false;
unsigned modal_waits = 0U;
void Check(bool ok, const char* message) {
    ++cases;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::wstring Text(CWnd& control) {
    CString value;
    control.GetWindowTextW(value);
    return value.GetString();
}
void CALLBACK InspectPreflight(HWND, UINT, UINT_PTR timer, DWORD) {
    const auto owner = test_app.m_pMainWnd->GetSafeHwnd();
    const auto dialog = GetLastActivePopup(owner);
    const auto list = dialog != owner ? GetDlgItem(dialog, ID_METADATA_DIFFERENCE_LIST) : nullptr;
    if (!list && ++modal_waits < 40U) return;
    KillTimer(nullptr, timer);
    if (!list) return;
    for (int row = 0; row < ListView_GetItemCount(list); ++row) {
        wchar_t field[128] = {}, current[256] = {}, action[256] = {}, result[32] = {};
        ListView_GetItemText(list, row, 0, field, 128);
        if (wcscmp(field, L"Host mode / destination") != 0) continue;
        ListView_GetItemText(list, row, 1, current, 256);
        ListView_GetItemText(list, row, 2, action, 256);
        ListView_GetItemText(list, row, 3, result, 32);
        preflight_checked = expected_boot ?
            wcsstr(current, L"BOOT") && !wcsstr(action, L"Enter") && wcscmp(result, L"MATCH") == 0 :
            wcsstr(current, L"APP") && wcsstr(action, L"Enter Bridge Boot") && wcscmp(result, L"MISMATCH") == 0;
    }
    PostMessageW(dialog, WM_COMMAND, IDOK, 0);
}
}

// Reuse the existing frame friend in a separate fixture executable.
struct CanPrerequisiteUiFixture {
static int Run(const wchar_t* image_path) {
    using namespace firmware_upgrade;
    if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_HIDE) || !AfxInitRichEdit2()) return 2;
    auto* frame = new CMainFrame();
    if (!frame->Create(nullptr, L"Owned Misc Host mode fixture", WS_OVERLAPPEDWINDOW, CRect(0,0,1366,768))) return 3;
    test_app.m_pMainWnd = frame;
    frame->worker_session_.StopAndJoin();
    while(frame->worker_session_.HasPendingEvents()) frame->worker_session_.DrainEvents();
    frame->worker_command_pending_ = false;
    frame->worker_started_ = false;
    frame->bus_tabs_.SetCurSel(3);
    frame->UpdateUi();
    const auto no_enter = [&] { return Text(frame->i2c_notes_).find(L"Enter Bridge Boot") == std::wstring::npos; };
    auto& state = frame->session_state_;
    auto& descriptor = state.bridge_info;
    auto loaded = std::make_shared<image_analysis::Result>();
    std::wstring error;
    Check(image_analysis::Load(image_path, {}, loaded.get(), &error), "load real BUR1 image through production inspector");
    ui::WorkerEvent event;
    event.kind = ui::WorkerEventKind::ImageLoaded;
    event.success = true;
    event.image_analysis = loaded;
    frame->ApplyLoadedImage(event);
    Check(frame->binary_bridge_inspection_.valid, "actual Host BIN passes metadata and CRC");
    frame->UpdateUi();
    Check(Text(frame->firmware_prerequisites_label_).find(L"Open + Probe Host") != std::wstring::npos && no_enter() &&
        !frame->enter_bootloader_btn_.IsWindowEnabled(), "offline BIN cannot establish a live Host mode");

    const auto& m = frame->binary_bridge_inspection_.metadata;
    descriptor.board_id = m.board_id;
    descriptor.boot_version = m.minimum_boot_version;
    descriptor.app_start = m.app_start; descriptor.capacity = m.capacity;
    descriptor.flash_size = m.flash_size; descriptor.boot_size = m.boot_size;
    descriptor.erase_size = m.erase_size; descriptor.minimum_size = m.minimum_size;
    descriptor.uart_baud = m.uart_baud; descriptor.irq_abi = m.irq_abi;
    std::memcpy(descriptor.family, m.family, sizeof(m.family));
    std::memcpy(descriptor.part, m.part, sizeof(m.part));
    descriptor.mode = BU_MODE_BOOT;
    descriptor.flags = BU_FLAG_FLASH_READY;
    state.serial_open = state.host_validated = state.bridge_info_valid = true;
    state.baudrate = m.uart_baud;
    Check(Bu_MatchesDevice(&m, &descriptor) != 0U, "synthetic descriptor matches actual inspected contract");

    const auto fits = [&] {
        LOGFONT base = {}; frame->ui_font_.GetLogFont(&base);
        for (int scale : {100, 125}) {
            auto spec = base; spec.lfHeight = MulDiv(base.lfHeight, scale, 100);
            CFont font; Check(font.CreateFontIndirect(&spec) != FALSE, "create owned scaled notes font");
            CClientDC dc(&frame->i2c_notes_); auto* old = dc.SelectObject(&font);
            CRect available; frame->i2c_notes_.GetClientRect(&available); available.DeflateRect(10,3,6,3);
            CRect measured = available;
            dc.DrawTextW(Text(frame->i2c_notes_).c_str(), measured, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
            dc.SelectObject(old);
            Check(measured.Height() <= available.Height() && measured.Width() <= available.Width(),
                "complete mode guidance fits actual normal/125-percent notes area");
        }
    };
    const auto capture = [&](const wchar_t* path) {
        CRect rect; frame->GetClientRect(&rect);
        CImage image; image.Create(rect.Width(), rect.Height(), 32);
        const auto dc = image.GetDC();
        frame->ShowWindow(SW_SHOWNOACTIVATE);
        frame->RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        frame->PrintWindow(CDC::FromHandle(dc), PW_CLIENTONLY);
        const auto pixel = GetPixel(dc, 2, 2);
        Check(pixel != RGB(0,0,0) && pixel != CLR_INVALID, "actual frame capture contains painted client pixels");
        image.ReleaseDC();
        frame->ShowWindow(SW_HIDE);
        Check(SUCCEEDED(image.Save(path)), "capture actual frame without COM or simulated drawing");
    };
    const auto inspected = frame->binary_bridge_inspection_;
    auto bytes = std::move(frame->binary_data_);
    frame->binary_bridge_inspection_ = {};
    frame->UpdateUi();
    Check(no_enter() && Text(frame->firmware_prerequisites_label_).find(L"Load a HOST APP") != std::wstring::npos &&
        !frame->start_firmware_update_btn_.IsWindowEnabled() && !frame->transfer_test_btn_.IsWindowEnabled(),
        "Boot first installation without BIN directs Load, with update and RUN disabled");
    frame->binary_data_ = std::move(bytes);
    frame->binary_bridge_inspection_ = inspected;
    frame->UpdateUi();
    Check(no_enter() && Text(frame->firmware_overview_cards_[1]).find(L"invalid / absent") != std::wstring::npos &&
        Text(frame->enter_bootloader_btn_) == L"Bridge Boot active" && !frame->enter_bootloader_btn_.IsWindowEnabled(),
        "first installation in Boot explicitly skips ENTER");
    Check(frame->start_firmware_update_btn_.IsWindowEnabled() && !frame->transfer_test_btn_.IsWindowEnabled(),
        "Boot without App permits matching update but forbids RUN");
    Check(Text(frame->firmware_prerequisites_label_).find(L"Ready: Update HOST APP.") != std::wstring::npos,
        "ready prerequisite identifies already-active Boot");
    fits(); capture(L"misc_boot_no_app.png");
    for (bool boot : {true, false}) {
        descriptor.mode = boot ? BU_MODE_BOOT : BU_MODE_APP;
        descriptor.flags = BU_FLAG_FLASH_READY | BU_FLAG_VALID_APP;
        descriptor.installed_size = m.image_size;
        frame->UpdateUi();
        Check(!!frame->enter_bootloader_btn_.IsWindowEnabled() == !boot &&
            !!frame->transfer_test_btn_.IsWindowEnabled() == boot &&
            !!frame->start_firmware_update_btn_.IsWindowEnabled() == boot,
            "App vs Boot preserves explicit ENTER/update/RUN gates");
        Check(boot ? no_enter() && Text(frame->firmware_prerequisites_label_).find(L"Ready:") == 0 :
            Text(frame->firmware_prerequisites_label_).find(L"Enter Bridge Boot (UART)") != std::wstring::npos,
            "only live App mode lists ENTER as the next update step");
        fits();
        preflight_checked = false; expected_boot = boot; modal_waits = 0U;
        const auto timer = SetTimer(nullptr, 0, 25, InspectPreflight);
        Check(timer != 0U, "create bounded preflight inspection timer");
        if (timer) { frame->OnPreflight(); KillTimer(nullptr, timer); }
        Check(preflight_checked, "actual Preflight distinguishes Boot direct update from App ENTER");
    }
    capture(L"misc_app_running.png");
    descriptor.mode = BU_MODE_BOOT;
    frame->last_update_result_ = feedback::Result::HostAppWritten;
    frame->UpdateUi();
    Check(no_enter() && Text(frame->firmware_prerequisites_label_).find(L"Next: Run Validated Bridge App") != std::wstring::npos &&
        Text(frame->i2c_notes_).find(L"Load HOST") == std::wstring::npos &&
        Text(frame->operation_status_card_).find(L"still in Boot") != std::wstring::npos,
        "completed write guides explicit RUN without repeating write/ENTER steps");
    fits();
    frame->last_update_result_ = feedback::Result::None;
    state.busy = true; frame->UpdateUi();
    Check(no_enter() && Text(frame->operation_status_card_).find(L"last verified BOOT") != std::wstring::npos &&
        !frame->start_firmware_update_btn_.IsWindowEnabled() && !frame->transfer_test_btn_.IsWindowEnabled(),
        "busy operation labels last observed mode and cannot authorize another action");
    state.busy = false;
    descriptor.flags = BU_FLAG_VALID_APP; frame->UpdateUi();
    Check(!frame->start_firmware_update_btn_.IsWindowEnabled() && no_enter(), "FLASH_READY remains required in Boot");
    ++descriptor.board_id; frame->UpdateUi();
    Check(!frame->start_firmware_update_btn_.IsWindowEnabled(), "mismatching Host BIN remains blocked");
    --descriptor.board_id;
    descriptor.mode = 0U; frame->UpdateUi();
    Check(no_enter() && !frame->enter_bootloader_btn_.IsWindowEnabled() &&
        !frame->start_firmware_update_btn_.IsWindowEnabled(), "unknown descriptor mode enables no write or ENTER");
    descriptor.mode = BU_MODE_BOOT; state.bridge_info_valid = false; frame->UpdateUi();
    Check(Text(frame->target_metadata_label_).find(L"self-update: N/A") != std::wstring::npos &&
        !frame->transfer_test_btn_.IsWindowEnabled(), "stale Boot descriptor is never treated as live");
    state = {};
    frame->worker_command_pending_ = false;
    frame->closing_ = true; // Expected load errors must not open modal dialogs.
    frame->worker_started_ = frame->worker_session_.Start(frame->GetSafeHwnd(), WM_APP + 1U);
    Check(frame->worker_started_, "restart actual image worker without opening COM");
    if (frame->worker_started_) {
        for (bool restored : {true, false}) {
            frame->SetProgress(100, L"Previous operation");
            Check(frame->LoadBinaryFromPath(image_path, restored), "queue startup restore/manual load through actual UI and worker");
            const auto deadline = GetTickCount64() + 5000U;
            while (frame->image_loading_ && GetTickCount64() < deadline) {
                frame->PumpWorkerEvents();
                Sleep(1U);
            }
            Check(!frame->image_loading_ && frame->progress_bar_.GetPos() == 0 &&
                !frame->progress_indeterminate_ && Text(frame->progress_label_) == L"Progress: 0%",
                "restore/manual analysis returns footer to idle, including marquee state");
            Check(frame->binary_bridge_inspection_.valid && frame->binary_data_.size() == m.image_size &&
                frame->last_update_result_ == feedback::Result::None && !state.serial_open,
                "offline BIN results remain available without fabricating update success or opening COM");
            Sleep(10U); // Let the completed command release its submission guard.
        }
        Check(frame->LoadBinaryFromPath(std::wstring(image_path) + L".missing", true), "queue missing restored image");
        const auto deadline = GetTickCount64() + 5000U;
        while (frame->image_loading_ && GetTickCount64() < deadline) {
            frame->PumpWorkerEvents();
            Sleep(1U);
        }
        Check(!frame->image_loading_ && frame->progress_bar_.GetPos() == 0 && !frame->progress_indeterminate_ &&
            frame->binary_data_.empty() && !frame->last_operation_error_.empty(),
            "failed restore also returns idle and keeps explicit error state");
        frame->worker_session_.StopAndJoin();
        frame->worker_started_ = false;
        frame->PumpWorkerEvents();
    }
    ui::WorkerEvent terminal;
    terminal.kind = ui::WorkerEventKind::OperationComplete;
    terminal.text = L"Image load canceled.";
    frame->image_loading_ = true;
    frame->SetProgress(0, L"Load BIN", true);
    frame->ApplyWorkerEvent(terminal);
    Check(!frame->image_loading_ && !frame->progress_indeterminate_ && frame->progress_bar_.GetPos() == 0 &&
        frame->last_update_result_ == feedback::Result::None, "canceled image completion cannot leave animation or a false100percent result");
    terminal.success = true;
    terminal.update_result = feedback::Result::HostAppWritten;
    frame->SetProgress(100, L"4/4 Host CRC/readback");
    frame->ApplyWorkerEvent(terminal);
    Check(frame->progress_bar_.GetPos() == 100 && frame->last_update_result_ == feedback::Result::HostAppWritten,
        "real update completion retains its100percent and explicit result");
    Check(TestSerialPortCatalog() == 0, "read-only COM catalog and existing numeric/PnP regression checks");
    frame->DestroyWindow(); test_app.m_pMainWnd = nullptr;
    std::cout << "Misc Host UI: " << cases << " checks, " << failures << " failures; no hardware\n";
    return failures ? 1 : 0;
}
};
int wmain(int argc, wchar_t** argv) { return argc == 2 ? CanPrerequisiteUiFixture::Run(argv[1]) : 2; }
