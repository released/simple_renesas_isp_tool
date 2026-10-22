// Native MFC fixture: actual frame/control paths, no COM commands or board I/O.
#include <afxcmn.h>
#include <afxwin.h>
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "../src/core/firmware_update_profile.h"
#include "../src/core/serial_port_catalog.h"
#include "../src/worker_session.h"
#include "../src/ui/console_geometry.h"
#include "../src/ui/status_card.h"
#include "../src/main_frame.h"

CWinApp test_app;
struct CanPrerequisiteUiFixture {
static int Run(int argc, wchar_t** argv) {
    if (argc != 3 || !AfxWinInit(GetModuleHandleW(nullptr),nullptr,GetCommandLineW(),SW_HIDE) ||
        !AfxInitRichEdit2()) return 2;
    int failures=0;
    const auto check=[&](bool ok,const char* reason) {
        if (!ok) {++failures;std::cerr<<"FAIL: "<<reason<<'\n';}
    };
    auto* frame=new CMainFrame();
    if (!frame->Create(nullptr,L"Offline prerequisite fixture",WS_OVERLAPPEDWINDOW,CRect(0,0,1366,768))) return 3;
    test_app.m_pMainWnd=frame;
    auto& state=frame->session_state_;
    state.host_validated=true;
    state.serial_open=true; // UI model only; actual SerialPort stays closed.
    state.host_capabilities.feature_flags=0xFFFFFFFFU;
    state.host_capabilities.nominal_bitrate_mask=7U;
    state.host_capabilities.data_bitrate_mask=7U;
    firmware_upgrade::bridge::Port port;
    port.flags=5U;port.rates=7U;port.maximum_tx=port.maximum_rx=64U;port.label=L"Offline CAN fixture";
    state.can_ports={port};
    frame->profile_combo_.SetCurSel(frame->profile_combo_.FindStringExact(-1,L"CAN Upgrade"));
    frame->can_mode_combo_.SetCurSel(0);
    frame->can_bitrate_combo_.SetCurSel(frame->can_bitrate_combo_.FindStringExact(-1,L"500K"));
    check(frame->LoadBinaryFromPath(argv[2],true),"actual FD BIN loads");
    frame->UpdateUi();
    const auto text=[&]() {CString value;frame->firmware_prerequisites_label_.GetWindowTextW(value);return std::wstring(value);};
    check(text().find(L"[BLOCKED] Selected CAN mode")==0,"mode mismatch takes priority");
    check(text().find(L"Check BIN supported CAN mode")!=std::wstring::npos,"action names BIN supported mode");
    check(text().find(L"Discover")==std::wstring::npos,"no contradictory discovery hint");
    check(frame->firmware_prerequisites_label_.emphasized_,"mismatch uses emphasized paint");
    check(frame->firmware_prerequisites_label_.tone_==firmware_upgrade::visual::Tone::Error,"mismatch remains error toned");
    check(!frame->connect_target_btn_.IsWindowEnabled() && !frame->start_firmware_update_btn_.IsWindowEnabled(),"existing discovery/update gates preserved");
    frame->last_update_result_=firmware_upgrade::feedback::Result::DeviceAppConfirmed;
    frame->UpdateUi();
    check(text().find(L"[BLOCKED]")==0,"previous completion never masks a current mismatch");
    frame->last_update_result_=firmware_upgrade::feedback::Result::None;
    const auto fits=[&](int scale) {
        LOGFONT spec={};frame->ui_font_.GetLogFont(&spec);
        spec.lfHeight=MulDiv(spec.lfHeight,scale,100);spec.lfWeight=FW_BOLD;
        CFont bold;bold.CreateFontIndirect(&spec);
        CClientDC dc(&frame->firmware_prerequisites_label_);
        auto* previous=dc.SelectObject(&bold);
        CRect available;frame->firmware_prerequisites_label_.GetClientRect(&available);
        available.DeflateRect(10,3,6,3);CRect measured=available;
        dc.DrawTextW(text().c_str(),measured,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
        dc.SelectObject(previous);
        return measured.Height()<=available.Height() && measured.Width()<=available.Width();
    };
    check(fits(100) && fits(125),"bold wrapped guidance fits fixture notice at normal/125-percent font size");
    frame->firmware_prerequisites_label_.SendMessage(WM_PAINT);
    const auto gdi_before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for (int i=0;i<300;++i) frame->firmware_prerequisites_label_.SendMessage(WM_PAINT);
    check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==gdi_before,"emphasized font paint restores/deletes temporary GDI objects");
    state.busy=true;frame->UpdateUi();
    check(text().find(L"Working:")==0 && !frame->firmware_prerequisites_label_.emphasized_,"active work retains working guidance");
    state.busy=false;
    frame->profile_combo_.SetCurSel(frame->profile_combo_.FindStringExact(-1,L"Custom Transfer"));
    frame->UpdateUi();
    check(text().find(L"Custom Transfer")==0 && !frame->firmware_prerequisites_label_.emphasized_,"Custom Transfer clears emphasis and ignores BIN mode");
    check(frame->connect_target_btn_.IsWindowEnabled(),"Custom Transfer discovery remains available");
    frame->profile_combo_.SetCurSel(frame->profile_combo_.FindStringExact(-1,L"CAN Upgrade"));
    check(frame->LoadBinaryFromPath(argv[1],true),"actual Classical BIN loads");
    frame->UpdateUi();
    check(text().find(L"Next: Discover Target")==0 && !frame->firmware_prerequisites_label_.emphasized_,"matching BIN restores normal discovery hint");
    check(frame->connect_target_btn_.IsWindowEnabled(),"matching BIN enables discovery");
    frame->can_bitrate_combo_.SetCurSel(frame->can_bitrate_combo_.FindStringExact(-1,L"250K"));
    frame->UpdateUi();
    check(text().find(L"[BLOCKED] Selected CAN bitrate")==0 && text().find(L"Discover")==std::wstring::npos,"bitrate mismatch has only actionable rate guidance");
    frame->can_bitrate_combo_.SetCurSel(frame->can_bitrate_combo_.FindStringExact(-1,L"500K"));
    frame->can_mode_combo_.SetCurSel(frame->can_mode_combo_.FindStringExact(-1,L"CAN FD"));
    frame->can_data_combo_.SetCurSel(frame->can_data_combo_.FindStringExact(-1,L"2M"));
    frame->UpdateUi();
    check(text().find(L"[BLOCKED] Selected CAN mode")==0,"reverse mismatch prioritizes mode over data-rate mismatch");
    check(frame->LoadBinaryFromPath(argv[2],true),"FD BIN reloads");
    frame->UpdateUi();
    check(text().find(L"Next: Discover Target")==0 && !frame->firmware_prerequisites_label_.emphasized_,"FD match clears previous mode/rate error");
    check(frame->connect_target_btn_.IsWindowEnabled(),"matching FD BIN enables discovery");
    state.can_started=true;state.target_validated=true;
    state.target_profile=firmware_upgrade::ui::TargetProfile::CanUpgrade;
    state.f13_bootloader_ready=false;
    frame->UpdateUi();
    check(text().find(L"Next: Enter Boot.")==0 && frame->firmware_prerequisites_label_.emphasized_,
        "discovered App emphasizes actionable Enter Boot next step");
    check(frame->enter_bootloader_btn_.IsWindowEnabled() && !frame->start_firmware_update_btn_.IsWindowEnabled(),
        "Enter Boot emphasis never bypasses Boot/update gate");
    check(frame->firmware_prerequisites_label_.tone_==firmware_upgrade::visual::Tone::Pending,
        "Enter Boot remains pending guidance rather than an image error");
    check(fits(100) && fits(125),"Enter Boot guidance fits at normal/125-percent font size");
    frame->can_mode_combo_.SetCurSel(0);frame->UpdateUi();
    check(text().find(L"[BLOCKED]")==0 && text().find(L"Enter Boot")==std::wstring::npos,
        "mode blocker still takes priority over Enter Boot");
    frame->can_mode_combo_.SetCurSel(1);
    state.f13_bootloader_ready=true;frame->UpdateUi();
    check(!frame->firmware_prerequisites_label_.emphasized_,"Boot mode clears Enter Boot emphasis");
    state.f13_bootloader_ready=false;state.busy=true;frame->UpdateUi();
    check(!frame->firmware_prerequisites_label_.emphasized_,"busy state clears Enter Boot emphasis");
    state.busy=false;
    state.can_started=false;state.target_validated=false;
    state.target_profile=firmware_upgrade::ui::TargetProfile::None;
    frame->latest_report_path_=L"offline-report-placeholder.html";
    frame->SetProgress(100,L"Device update complete");
    frame->last_update_result_=firmware_upgrade::feedback::Result::DeviceAppConfirmed;
    firmware_upgrade::ui::WorkerEvent connection;
    connection.kind=firmware_upgrade::ui::WorkerEventKind::State;
    connection.state=state;
    frame->ApplyWorkerEvent(connection);
    check(frame->progress_bar_.GetPos()==100,"same-connection idle state preserves completed progress");
    connection.state.serial_open=false;connection.state.host_validated=false;
    frame->ApplyWorkerEvent(connection);
    connection.state.serial_open=true;connection.state.host_validated=true;
    frame->ApplyWorkerEvent(connection);
    CString progress_text;frame->progress_label_.GetWindowTextW(progress_text);
    check(frame->progress_bar_.GetPos()==0 && progress_text==L"Progress: 0%",
        "disconnect/reconnect clears old percentage and operation caption");
    check(frame->last_update_result_==firmware_upgrade::feedback::Result::None &&
        frame->latest_report_path_==L"offline-report-placeholder.html",
        "new connection clears stale completion without losing last report");
    frame->SetProgress(37,L"3/5 Transfer");
    connection.state.busy=true;frame->ApplyWorkerEvent(connection);
    check(frame->progress_bar_.GetPos()==37,"same-connection busy state does not erase active progress");
    connection.state.busy=false;frame->ApplyWorkerEvent(connection);
    frame->bus_tabs_.SetCurSel(2);frame->UpdateUi();
    check(!frame->firmware_prerequisites_label_.emphasized_,"Misc does not inherit CAN emphasis");
    frame->bus_tabs_.SetCurSel(1);frame->UpdateUi();
    check(!frame->firmware_prerequisites_label_.emphasized_,"I2C does not inherit CAN emphasis");
    frame->worker_session_.StopAndJoin();
    frame->DestroyWindow();test_app.m_pMainWnd=nullptr;
    std::cout<<"Actual MFC prerequisite/frame/font-state tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
};
int wmain(int argc, wchar_t** argv) {
    return CanPrerequisiteUiFixture::Run(argc,argv);
}
