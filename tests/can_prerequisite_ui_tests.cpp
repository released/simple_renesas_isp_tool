// Native MFC fixture: actual frame/control paths, no COM commands or board I/O.
#include <afxcmn.h>
#include <afxwin.h>
#include <atlimage.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include "../src/core/firmware_update_profile.h"
#include "../src/core/serial_port_catalog.h"
#include "../src/core/operation_guidance.h"
#include "../src/worker_session.h"
#include "../src/ui/console_geometry.h"
#include "../src/ui/status_card.h"
#include "../src/main_frame.h"
#include "../src/resource.h"

CWinApp test_app;
namespace {
bool details_dialog_checked=false,details_dialog_readonly=false,details_dialog_captured=false;
unsigned details_waits=0U;
void CALLBACK CheckDetailsDialog(HWND,UINT,UINT_PTR timer,DWORD) {
    const auto parent=test_app.m_pMainWnd->GetSafeHwnd();
    const auto dialog=GetLastActivePopup(parent);
    if(dialog==parent||GetWindowThreadProcessId(dialog,nullptr)!=GetCurrentThreadId())return;
    const auto list=GetDlgItem(dialog,ID_METADATA_DIFFERENCE_LIST);
    if(!list&&++details_waits<50U)return;
    KillTimer(nullptr,timer);
    if(list) {
        details_dialog_checked=ListView_GetItemCount(list)==22&&Header_GetItemCount(ListView_GetHeader(list))==2;
        details_dialog_readonly=(GetWindowLongPtrW(list,GWL_STYLE)&LVS_EDITLABELS)==0;
        ::ShowWindow(dialog,SW_SHOWNOACTIVATE);
        ::RedrawWindow(dialog,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_ERASE);
        CRect area;::GetClientRect(dialog,&area);CImage snapshot;snapshot.Create(area.Width(),area.Height(),32);
        const auto dc=snapshot.GetDC();const auto source=::GetDC(dialog);
        ::BitBlt(dc,0,0,area.Width(),area.Height(),source,0,0,SRCCOPY);::ReleaseDC(dialog,source);
        details_dialog_captured=::GetPixel(dc,2,2)!=RGB(0,0,0)&&::GetPixel(dc,2,2)!=CLR_INVALID;
        snapshot.ReleaseDC();snapshot.Save(L"bin_details_fixture.png");
    }
    ::SendMessageW(dialog,WM_COMMAND,IDOK,0);
}
}
struct CanPrerequisiteUiFixture {
static int Run(int argc, wchar_t** argv) {
    if ((argc<3||argc>6) || !AfxWinInit(GetModuleHandleW(nullptr),nullptr,GetCommandLineW(),SW_HIDE) ||
        !AfxInitRichEdit2()) return 2;
    int failures=0;
    const auto check=[&](bool ok,const char* reason) {
        if (!ok) {++failures;std::cerr<<"FAIL: "<<reason<<'\n';}
    };
    auto* frame=new CMainFrame();
    if (!frame->Create(nullptr,L"Offline prerequisite fixture",WS_OVERLAPPEDWINDOW,CRect(0,0,1366,768))) return 3;
    test_app.m_pMainWnd=frame;
    auto& state=frame->session_state_;
    check(firmware_upgrade::guidance::Explain(L"Device UART reply timeout").next.find(L"115200")==std::wstring::npos&&
        firmware_upgrade::guidance::Explain(L"Selected device bus rate differs from live INFO").layer==L"Device bus settings",
        "device diagnostics use selected settings rather than fixed115200 advice");
    check(frame->BuildBinDetails().empty()&&!frame->bin_details_btn_.IsWindowEnabled(),"BIN details unavailable without an image");
    const auto compact_layout=[&]() {
        unsigned visible=0U;
        for(auto& field:frame->firmware_field_values_)if(field.GetStyle()&WS_VISIBLE)++visible;
        check(visible==12U,"compact overview shows12 of20 cached fields");
        CRect host,device,boot,app,bin,button;
        frame->host_info_label_.GetWindowRect(&host);frame->target_info_label_.GetWindowRect(&device);
        frame->firmware_overview_cards_[0].GetWindowRect(&boot);frame->firmware_overview_cards_[1].GetWindowRect(&app);
        frame->firmware_overview_cards_[2].GetWindowRect(&bin);frame->bin_details_btn_.GetWindowRect(&button);
        check(host.top==device.top&&device.top==boot.top&&boot.top==app.top,"four live cards share first row");
        check(bin.top>=host.bottom&&bin.left==host.left&&button.top>=bin.bottom,"Loaded BIN and details button occupy second row");
    };
    compact_layout();
    for(const auto size:{std::pair<int,int>{1244,661},{1350,690},{1904,990}})for(const int metric:{14,18,21}) {
        const auto r=firmware_upgrade::console::Calculate(size.first,(std::max)(size.second,firmware_upgrade::console::MinimumClientHeight(metric)),metric);
        const auto inside=[&](const firmware_upgrade::console::Rect& c){return c.x>=r.analysis_group.x&&c.y>=r.analysis_group.y&&
            c.x+c.width<=r.analysis_group.x+r.analysis_group.width&&c.y+c.height<=r.analysis_group.y+r.analysis_group.height;};
        check(r.fits&&inside(r.loaded_bin)&&inside(r.bin_details),"compact font-aware geometry contains second-row BIN and button");
        for(const auto& group:r.metadata_groups)check(inside(group)&&group.x>r.loaded_bin.x+r.loaded_bin.width&&group.y==r.loaded_bin.y,
            "three metadata groups align beside BIN without overlap");
    }
    using firmware_upgrade::visual::Provenance;
    check(frame->host_info_label_.provenance_==Provenance::HostLive&&
        frame->target_info_label_.provenance_==Provenance::DeviceLive&&
        frame->firmware_overview_cards_[0].provenance_==Provenance::DeviceLive&&
        frame->firmware_overview_cards_[1].provenance_==Provenance::DeviceLive&&
        frame->firmware_overview_cards_[2].provenance_==Provenance::ImageFile,
        "Host/Device/offline BIN have independent provenance, related device cards match");
    check(firmware_upgrade::visual::SourceColors(Provenance::HostLive).border!=
        firmware_upgrade::visual::SourceColors(Provenance::DeviceLive).border&&
        firmware_upgrade::visual::SourceColors(Provenance::DeviceLive).border!=
        firmware_upgrade::visual::SourceColors(Provenance::ImageFile).border,"provenance palettes differ");
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
    const auto verify_bin_card=[&](const wchar_t* snapshot) {
        auto& card=frame->firmware_overview_cards_[2];
        check(card.compact_detail_&&!frame->firmware_overview_cards_[0].compact_detail_&&
            !frame->host_info_label_.compact_detail_,"only Loaded BIN uses compact detail rows");
        CString complete;card.GetWindowTextW(complete);
        const auto lines=card.CompactDetailLines();
        check(lines.size()<=2U,"Loaded BIN paints at most four complete rows including heading/value");
        if(std::count(card.detail_.begin(),card.detail_.end(),L'\n')>=2)
            check(lines.size()==2U&&lines.back().find(L"...")!=std::wstring::npos,
                "additional route diagnostics are explicitly abbreviated, not partly drawn");
        LOGFONT base={};frame->ui_font_.GetLogFont(&base);
        CRect client;frame->GetClientRect(&client);
        card.SendMessage(WM_PAINT); // Initialize lazy window/GDI resources before leak checks.
        const auto gdi_count=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(const int scale:{100,125,150}) {
            auto spec=base;spec.lfHeight=MulDiv(base.lfHeight,scale,100);
            CFont scaled;check(scaled.CreateFontIndirect(&spec)!=FALSE,"BIN font fixture created");
            card.SetFont(&scaled);
            {
                CClientDC dc(&card);auto* previous=dc.SelectObject(&scaled);
                TEXTMETRIC metric={};dc.GetTextMetrics(&metric);
                for(const int width:{1244,1350,1904}) {
                    const auto layout=firmware_upgrade::console::Calculate(width,
                        firmware_upgrade::console::MinimumClientHeight(metric.tmHeight),metric.tmHeight);
                    const auto& r=layout.loaded_bin;
                    card.MoveWindow(r.x,r.y,r.width,r.height,FALSE);
                    CRect available;card.GetClientRect(&available);available.DeflateRect(10,3,6,3);
                    const int line=metric.tmHeight+1;
                    check((2+static_cast<int>(lines.size()))*line<=available.Height(),
                        "complete BIN text rows fit actual100/125/150-percent fonts at three widths");
                    for(const auto& detail:lines) {
                        CRect measured=available;
                        dc.DrawTextW(detail.c_str(),measured,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
                        check(measured.Height()<=line,"BIN detail paragraphs cannot wrap into a clipped fifth row");
                    }
                    card.SendMessage(WM_PAINT);
                    LOGFONT retained_font={};
                    check(::GetObjectW(scaled.GetSafeHandle(),sizeof(retained_font),&retained_font)==sizeof(retained_font),
                        "painting never deletes the control's borrowed font");
                }
                dc.SelectObject(previous);
            }
            card.SetFont(&frame->ui_font_);
        }
        const auto gdi_after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        if(gdi_after!=gdi_count)std::cerr<<"BIN GDI before="<<gdi_count<<" after="<<gdi_after<<'\n';
        check(gdi_after==gdi_count,"compact BIN paint preserves font lifetime/GDI count");
        CString retained;card.GetWindowTextW(retained);
        check(retained==complete,"compact drawing preserves complete accessible/tooltip content");
        frame->LayoutControls(client.Width(),client.Height());
        CImage image;image.Create(client.Width(),client.Height(),32);
        const auto paint=image.GetDC();frame->ShowWindow(SW_SHOWNOACTIVATE);frame->UpdateWindow();
        frame->PrintWindow(CDC::FromHandle(paint),PW_CLIENTONLY);
        image.ReleaseDC();frame->ShowWindow(SW_HIDE);
        check(SUCCEEDED(image.Save(snapshot)),"save private actual compact BIN frame snapshot");
    };
    verify_bin_card(L"loaded_bin_valid.png");
    const auto text=[&]() {CString value;frame->firmware_prerequisites_label_.GetWindowTextW(value);return std::wstring(value);};
    const auto details=frame->BuildBinDetails();
    check(details.size()==22U&&details.front().image==argv[2]&&details.back().image!=L"PASS","details preserve full file path and wrong-route result");
    CString calculated;frame->firmware_field_values_[CMainFrame::kFirmwareFieldCalculatedCrc].GetWindowTextW(calculated);
    check(details[2].image==calculated.GetString()&&!(frame->firmware_field_values_[CMainFrame::kFirmwareFieldCalculatedCrc].GetStyle()&WS_VISIBLE),
        "hidden calculated CRC remains available untruncated in details");
    check(frame->bin_details_btn_.IsWindowEnabled(),"offline details require neither matching route nor a live device");
    const auto dialog_timer=::SetTimer(nullptr,0,50,CheckDetailsDialog);
    check(dialog_timer!=0,"details modal test timer created");
    if(dialog_timer){frame->OnBinDetails();::KillTimer(nullptr,dialog_timer);}
    check(details_dialog_checked&&details_dialog_readonly,"actual BIN details modal shows22 rows/two readonly columns");
    check(details_dialog_captured,"actual visible details client capture contains painted pixels");
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
    check(!frame->bin_details_btn_.IsWindowEnabled(),"details cannot interrupt active work");
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
    frame->bus_tabs_.SetCurSel(3);frame->UpdateUi();
    compact_layout();
    check(!frame->firmware_prerequisites_label_.emphasized_,"Misc does not inherit CAN emphasis");
    frame->bus_tabs_.SetCurSel(1);frame->i2c_workflow_combo_.SetCurSel(2);frame->UpdateUi();
    check(!frame->firmware_prerequisites_label_.emphasized_,"I2C does not inherit CAN emphasis");
    if(argc>=4&&argv[3][0]) {
        state.busy=false;state.can_started=false;state.uart_valid=false;
        state.host_validated=true;state.serial_open=true;
        auto uart_port=port;uart_port.bus=firmware_upgrade::bridge::Bus::Uart;
        uart_port.flags=1U;uart_port.rates=1U;uart_port.maximum_rx=uart_port.maximum_tx=512U;
        uart_port.label=L"UART1 TX74/RX75";state.uart_ports={uart_port};
        frame->bus_tabs_.SetCurSel(2);frame->uart_workflow_combo_.SetCurSel(0);
        check(frame->LoadBinaryFromPath(argv[3],true),"actual UART BIN loads");frame->UpdateUi();
        check(frame->IsUartTab()&&!frame->IsMiscTab(),"UART inserted before Misc");
        check(frame->binary_image_valid_&&text().find(L"Next: Discover Target")==0,"UART BIN uses independent validator and next step");
        check(frame->connect_target_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),"UART discover before write");
        const auto& m=frame->binary_inspection_.metadata;auto& t=state.uart_target;
        t.mode=ISP_MODE_APP;t.valid=true;t.ready=true;t.baud=115200U;t.target=m.target_id;
        t.start=m.application_start;t.capacity=m.application_capacity;t.flash=m.code_flash_size;
        t.boot=m.boot_reserved_size;t.erase=m.erase_block_size;t.irq_abi=m.policy.irq_abi;
        t.boot_version=m.policy.minimum_boot_version;t.part=m.mcu_part;t.family=m.mcu_family;
        t.version=m.version;t.build=m.build;t.size=m.image_size;
        state.uart_valid=true;state.uart_configuration={};frame->UpdateUi();
        check(text().find(L"Next: Enter Boot")==0&&frame->firmware_prerequisites_label_.emphasized_,"UART App emphasizes Enter Boot");
        check(frame->enter_bootloader_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),"UART App cannot write");
        t.mode=ISP_MODE_BOOT;frame->UpdateUi();
        check(frame->start_firmware_update_btn_.IsWindowEnabled()&&text().find(L"Ready: Start Device Update")==0,"UART Boot matching image ready");
        state.uart_ports[0].rates=DEVICE_UART_RATE_MASK;
        frame->uart_baud_combo_.SetWindowTextW(L"123456");frame->OnUartSettingsChanged();
        firmware_upgrade::isp::BusConfiguration chosen;
        check(frame->SelectedUartSettings(&chosen)&&chosen.baud==123456U&&
            !frame->IsUartSelectionCurrent()&&!frame->start_firmware_update_btn_.IsWindowEnabled()&&
            frame->connect_target_btn_.IsWindowEnabled(),"custom UART rate editable,old discovery invalidated");
        check(frame->ReadIniValue(L"device_uart_baud",L"")==L"123456","custom device UART baud persists independently in INI");
        state.uart_valid=true;state.uart_configuration=chosen;t.baud=123456U;frame->UpdateUi();
        check(frame->IsUartSelectionCurrent()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),
            "fresh custom-rate live target still blocks a default-rate BIN");
        verify_bin_card(L"custom_uart_settings.png");
        frame->uart_baud_combo_.SetWindowTextW(L"bad");frame->OnUartSettingsChanged();
        check(frame->uart_baud_combo_.IsWindowEnabled()&&!frame->connect_target_btn_.IsWindowEnabled(),
            "invalid baud stays editable and blocks discovery");
        frame->uart_baud_combo_.SetWindowTextW(L"115200");frame->OnUartSettingsChanged();
        check(!frame->IsUartSelectionCurrent(),"returning to old baud still requires fresh discovery");
        t.baud=115200U;state.uart_valid=true;state.uart_configuration={};frame->UpdateUi();
        {
            CRect area;frame->GetClientRect(&area);CImage image;
            image.Create(area.Width(),area.Height(),32);
            frame->ShowWindow(SW_SHOWNOACTIVATE);frame->UpdateWindow();
            const HDC dc=image.GetDC();
            frame->PrintWindow(CDC::FromHandle(dc),PW_CLIENTONLY);
            image.ReleaseDC();
            frame->ShowWindow(SW_HIDE);
            check(SUCCEEDED(image.Save(L"uart_offline_fixture.png")),"save private native UART layout snapshot");
        }
        check(fits(100)&&fits(125),"UART next step fits normal/125-percent font");
        t.version_control=true;t.version=m.version+1U;frame->UpdateUi();
        check(!frame->start_firmware_update_btn_.IsWindowEnabled()&&frame->force_old_firmware_btn_.IsWindowEnabled(),"UART downgrade uses explicit force action");
        state.busy=true;frame->UpdateUi();
        check(!frame->connect_target_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled()&&frame->cancel_btn_.IsWindowEnabled(),"UART independent cancel while busy");
        state.busy=false;t.version=m.version;state.uart_valid=false;
        frame->last_update_result_=firmware_upgrade::feedback::Result::UartAppConfirmed;frame->UpdateUi();
        check(text().find(L"App confirmed;")==0&&text().find(L"CAN")==std::wstring::npos,"UART completion avoids CAN hints after target released");
        frame->last_update_result_=firmware_upgrade::feedback::Result::None;
        frame->LoadBinaryFromPath(argv[1],true);frame->UpdateUi();
        check(!frame->binary_image_valid_&&text().find(L"[BLOCKED] Check BIN update interface")==0,"CAN BIN cannot enter UART update");
        check(frame->firmware_overview_cards_[2].heading_.find(L"WRONG ROUTE")!=std::wstring::npos&&
            std::count(frame->firmware_overview_cards_[2].detail_.begin(),frame->firmware_overview_cards_[2].detail_.end(),L'\n')>=2,
            "actual wrong-route image retains full multi-paragraph policy diagnostic");
        verify_bin_card(L"loaded_bin_wrong_route.png");
        frame->uart_workflow_combo_.SetCurSel(1);t={};t.mode=ISP_MODE_TRANSFER;t.baud=115200U;
        t.part=L"UART fixture";state.uart_valid=true;frame->UpdateUi();
        check(frame->connect_target_btn_.IsWindowEnabled()&&frame->transfer_test_btn_.IsWindowEnabled()&&
            !frame->start_firmware_update_btn_.IsWindowEnabled(),"UART Custom Transfer ignores BIN, never enables flash");
        frame->bus_tabs_.SetCurSel(3);frame->UpdateUi();
        check(frame->IsMiscTab()&&!frame->IsUartTab(),"Misc index and role remain separate");
        frame->bus_tabs_.SetCurSel(0);frame->UpdateUi();
        check(text().find(L"UART")==std::wstring::npos,"CAN restored without UART hints");
    }
    if(argc>=5&&argv[4][0]) {
        state.host_validated=true;state.serial_open=true;state.can_started=false;state.busy=false;state.uart_valid=false;
        auto i2c_port=port;i2c_port.bus=firmware_upgrade::bridge::Bus::I2c;
        i2c_port.maximum_rx=i2c_port.maximum_tx=512U;i2c_port.flags=1U;i2c_port.rates=1U;i2c_port.label=L"IICA0 SCL62/SDA63";
        state.i2c_ports={i2c_port};state.isp_contract=2U;
        frame->bus_tabs_.SetCurSel(1);frame->i2c_workflow_combo_.SetCurSel(0);
        check(frame->LoadBinaryFromPath(argv[4],true),"actual I2C BIN loads");frame->UpdateUi();
        check(frame->binary_image_valid_&&frame->IsI2cIsp()&&text().find(L"Next: Discover Target")==0,"I2C ISP independent image and workflow");
        check((frame->connect_target_btn_.GetStyle()&WS_VISIBLE)&&frame->connect_target_btn_.IsWindowEnabled(),"I2C ISP discovery has visible style and is enabled");
        const auto& m=frame->binary_inspection_.metadata;auto& t=state.uart_target;t={};t.contract=2U;t.transport=IP_I2C;
        t.mode=ISP_MODE_APP;t.valid=true;t.ready=true;t.baud=100000U;t.target=m.target_id;
        t.start=m.application_start;t.capacity=m.application_capacity;t.flash=m.code_flash_size;t.boot=m.boot_reserved_size;
        t.erase=m.erase_block_size;t.irq_abi=m.policy.irq_abi;t.boot_version=m.policy.minimum_boot_version;t.part=m.mcu_part;t.family=m.mcu_family;
        t.version=m.version;t.build=m.build;t.size=m.image_size;state.uart_valid=true;state.uart_configuration={0U,100000U};
        frame->UpdateUi();check(text().find(L"Next: Enter Boot")==0&&frame->firmware_prerequisites_label_.emphasized_,"I2C App next step is bold Enter Boot");
        t.mode=ISP_MODE_BOOT;frame->UpdateUi();check(frame->start_firmware_update_btn_.IsWindowEnabled(),"I2C Boot matching image ready");
        state.i2c_ports[0].rates=3U;frame->UpdateUi();
        frame->i2c_address_edit_.SetWindowTextW(L"0x42");frame->OnI2cSettingsChanged();
        frame->i2c_speed_combo_.SetCurSel(1);frame->OnI2cSettingsChanged();
        firmware_upgrade::isp::BusConfiguration chosen;
        check(frame->SelectedUartSettings(&chosen)&&chosen.address==0x42U&&chosen.baud==400000U&&
            frame->i2c_address_edit_.IsWindowEnabled()&&frame->i2c_speed_combo_.IsWindowEnabled()&&
            !frame->IsUartSelectionCurrent()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),
            "custom I2C7-bit address/400K retained and invalidate stale discovery");
        check(frame->ReadIniValue(L"i2c_address",L"")==L"0x42"&&frame->ReadIniValue(L"i2c_speed",L"")==L"1",
            "I2C address/speed persisted");
        state.uart_valid=true;state.uart_configuration=chosen;t.address=0x42U;t.baud=400000U;frame->UpdateUi();
        check(frame->IsUartSelectionCurrent()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),
            "fresh400K/custom-address target blocks default100K/20 BIN");
        verify_bin_card(L"custom_i2c_settings.png");
        frame->i2c_address_edit_.SetWindowTextW(L"0x80");frame->OnI2cSettingsChanged();
        check(frame->i2c_address_edit_.IsWindowEnabled()&&!frame->connect_target_btn_.IsWindowEnabled(),
            "shifted/reserved I2C address blocked without disabling edit");
        frame->i2c_address_edit_.SetWindowTextW(L"0x20");frame->i2c_speed_combo_.SetCurSel(0);frame->OnI2cSettingsChanged();
        t.address=0x20U;t.baud=100000U;state.uart_valid=true;state.uart_configuration={0U,100000U};frame->UpdateUi();
        t.version_control=true;t.version=m.version+1U;frame->UpdateUi();
        check((frame->force_old_firmware_btn_.GetStyle()&WS_VISIBLE)&&frame->force_old_firmware_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),"I2C version floor and explicit force");
        check(fits(100)&&fits(125),"I2C hint fits normal/125-percent fonts");
        t.version=m.version;t.version_control=false;frame->UpdateUi();
        {
            CRect area;frame->GetClientRect(&area);CImage image;image.Create(area.Width(),area.Height(),32);
            HDC dc=image.GetDC();frame->ShowWindow(SW_SHOWNOACTIVATE);frame->UpdateWindow();frame->PrintWindow(CDC::FromHandle(dc),PW_CLIENTONLY);
            image.ReleaseDC();frame->ShowWindow(SW_HIDE);check(SUCCEEDED(image.Save(L"i2c_offline_fixture.png")),"save private I2C native layout");
        }
        state.busy=true;frame->UpdateUi();check(frame->cancel_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),"I2C independent cancel");state.busy=false;
        frame->i2c_workflow_combo_.SetCurSel(1);t={};t.contract=2U;t.transport=IP_I2C;t.mode=ISP_MODE_TRANSFER;
        state.uart_valid=true;frame->UpdateUi();check(frame->transfer_test_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),"I2C Transfer never enables Flash");
        frame->bus_tabs_.SetCurSel(2);frame->uart_workflow_combo_.SetCurSel(2);frame->LoadBinaryFromPath(argv[3],true);frame->UpdateUi();
        check(!frame->binary_image_valid_&&!frame->IsUartSelectionCurrent(),"Xmodem never reuses UART BIN/live session");
        frame->bus_tabs_.SetCurSel(1);frame->i2c_workflow_combo_.SetCurSel(2);frame->UpdateUi();
        check(!frame->start_firmware_update_btn_.IsWindowEnabled(),"G16 reference remains read-only");
    }
    if(argc==6&&argv[5][0]) {
        state.host_validated=true;state.serial_open=true;state.can_started=false;state.busy=false;
        frame->bus_tabs_.SetCurSel(2);frame->uart_workflow_combo_.SetCurSel(2);
        check(frame->LoadBinaryFromPath(argv[5],true),"actual Xmodem BIN loads");frame->UpdateUi();
        check(frame->SelectedIspContract()==3U&&frame->binary_image_valid_,"UART workflow selects XMODEM-CRC with matching image");
        state.isp_contract=3U;state.uart_configuration={0U,115200U};
        const auto& m=frame->binary_inspection_.metadata;auto& t=state.uart_target;t={};
        t.contract=3U;t.transport=IP_UART;t.mode=ISP_MODE_APP;t.valid=t.ready=true;t.baud=115200U;
        t.target=m.target_id;t.start=m.application_start;t.capacity=m.application_capacity;t.flash=m.code_flash_size;
        t.boot=m.boot_reserved_size;t.erase=m.erase_block_size;t.irq_abi=m.policy.irq_abi;t.boot_version=m.policy.minimum_boot_version;
        t.part=m.mcu_part;t.family=m.mcu_family;t.version=m.version;t.build=m.build;t.size=m.image_size;
        state.uart_valid=true;frame->UpdateUi();
        check(frame->enter_bootloader_btn_.IsWindowEnabled()&&text().find(L"Next: Enter Boot")==0,"Xmodem App next action is Enter Boot");
        t.mode=ISP_MODE_BOOT;frame->UpdateUi();
        check(frame->start_firmware_update_btn_.IsWindowEnabled(),"matching Xmodem Boot enables update");
        frame->uart_workflow_combo_.SetCurSel(3);t.mode=ISP_MODE_TRANSFER;frame->UpdateUi();
        check(frame->transfer_test_btn_.IsWindowEnabled()&&!frame->start_firmware_update_btn_.IsWindowEnabled(),"Xmodem Transfer has no Flash permission");
        TCITEM item={};wchar_t tab_text[64]={};item.mask=TCIF_TEXT;item.pszText=tab_text;item.cchTextMax=64;
        frame->bus_tabs_.GetItem(2,&item);check(std::wstring(tab_text)==L"UART / Xmodem","Xmodem visible in tab caption");
        CRect tab_bounds,last_tab;frame->bus_tabs_.GetClientRect(&tab_bounds);frame->bus_tabs_.GetItemRect(3,&last_tab);
        check(last_tab.right<=tab_bounds.right,"all four naturally-sized tab captions fit left column");
        const auto before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(int i=0;i<100;++i)for(auto* card:{&frame->host_info_label_,&frame->target_info_label_,
            &frame->firmware_overview_cards_[0],&frame->firmware_overview_cards_[1],&frame->firmware_overview_cards_[2]})card->SendMessage(WM_PAINT);
        check(before==GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),"provenance paint restores fonts/brushes");
        frame->uart_workflow_combo_.SetCurSel(2);t.mode=ISP_MODE_APP;frame->UpdateUi();
        CRect area;frame->GetClientRect(&area);CImage screenshot;screenshot.Create(area.Width(),area.Height(),32);
        HDC paint=screenshot.GetDC();frame->ShowWindow(SW_SHOWNOACTIVATE);frame->UpdateWindow();frame->PrintWindow(CDC::FromHandle(paint),PW_CLIENTONLY);
        screenshot.ReleaseDC();frame->ShowWindow(SW_HIDE);check(SUCCEEDED(screenshot.Save(L"serial_provenance_fixture.png")),"save native Xmodem/provenance fixture");
        frame->bus_tabs_.SetCurSel(3);frame->UpdateUi();
        check(frame->target_info_label_.provenance_==Provenance::HostLive&&frame->firmware_overview_cards_[0].provenance_==Provenance::HostLive&&
            frame->firmware_overview_cards_[1].provenance_==Provenance::HostLive&&frame->firmware_overview_cards_[2].provenance_==Provenance::ImageFile,
            "Misc switches all live bridge cards to Host provenance, not Device");
    }
    frame->firmware_overview_cards_[2].SetContent(L"LOADED APP BIN | OFFLINE | INVALID",L"123 B",
        L"Invalid metadata: a long diagnostic remains readable in the complete detail snapshot.\r\n"
        L"Check the selected image and its metadata.\r\nAdditional diagnostic context is retained.");
    verify_bin_card(L"loaded_bin_invalid.png");
    frame->worker_session_.StopAndJoin();
    frame->DestroyWindow();test_app.m_pMainWnd=nullptr;
    std::cout<<"Actual MFC prerequisite/frame/font-state tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
};
int wmain(int argc, wchar_t** argv) {
    return CanPrerequisiteUiFixture::Run(argc,argv);
}
