// Native closed-field bounds, popup extents and labels; no COM opens.
#include <afxwin.h>
#include <afxcmn.h>
#include <atlimage.h>
#include <iostream>
#include <vector>
#include "../src/main_frame.h"
#include "../src/resource.h"

CWinApp test_app;
struct CanPrerequisiteUiFixture {
    static int Run() {
        if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_HIDE) || !AfxInitRichEdit2()) return 2;
        unsigned failures=0U, cases=0U, old_overlaps=0U;
        const auto check=[&](bool okay,const char* reason) {
            if (!okay) {++failures;std::cerr<<"FAIL "<<reason<<'\n';}
        };
        // Restore the selected tab before Create, then follow the production
        // first-show/maximize path without a compensating UpdateUi or tab switch.
        wchar_t module[MAX_PATH]={};
        if(!GetModuleFileNameW(nullptr,module,MAX_PATH))return 6;
        std::wstring ini=module;ini.resize(ini.find_last_of(L"\\/")+1U);ini+=L"FirmwareUpgradeTool.ini";
        const HHOOK guard=SetWindowsHookExW(WH_CBT,[](int code,WPARAM w,LPARAM l)->LRESULT {
            return code==HCBT_ACTIVATE?1:CallNextHookEx(nullptr,code,w,l);
        },nullptr,GetCurrentThreadId());
        if(!guard)return 7;
        for(int initial=0;initial<4;++initial) {
            const bool uart=initial<2;
            check(WritePrivateProfileStringW(L"ui",L"bus_tab",uart?L"UART":L"I2C",ini.c_str())!=FALSE,"seed private startup tab");
            check(WritePrivateProfileStringW(L"ui",L"device_uart_baud",initial==1?L"123457":L"115200",ini.c_str())!=FALSE,"seed private UART baud");
            check(WritePrivateProfileStringW(L"ui",L"i2c_speed",initial==3?L"1":nullptr,ini.c_str())!=FALSE,"seed default/saved I2C speed");
            auto* frame=new CMainFrame();
            if(!frame->Create(nullptr,L"Initial console display - no COM",WS_OVERLAPPEDWINDOW,CRect(0,0,1366,728)))return 8;
            test_app.m_pMainWnd=frame;
            frame->ShowWindow(SW_SHOWMAXIMIZED);frame->UpdateWindow();
            CString value;
            auto& field=uart?frame->uart_baud_combo_:frame->i2c_speed_combo_;
            field.GetWindowTextW(value);
            check(frame->bus_tabs_.GetCurSel()==(uart?2:1),"startup restores the selected tab");
            check(!field.IsWindowEnabled(),"offline default field stays disabled");
            if(uart) {
                const DWORD selection=field.GetEditSel();
                check(LOWORD(selection)==HIWORD(selection),"first UART show has no selected text before any refresh");
                check(value==(initial==1?L"123457":L"115200"),"first UART show preserves default/custom baud");
                std::cout<<"STARTUP UART "<<initial<<" selection="<<LOWORD(selection)<<":"<<HIWORD(selection)<<"\n";
            } else {
                check(value==(initial==3?L"400 kHz":L"100 kHz"),"first offline I2C show displays default/saved speed");
                check(!frame->i2c_probe_btn_.IsWindowEnabled()&&!frame->connect_target_btn_.IsWindowEnabled(),"offline I2C display grants no discovery/probe permission");
                std::wcout<<L"STARTUP I2C "<<initial<<L" value="<<value.GetString()<<L"\n";
            }
            check(::GetForegroundWindow()!=frame->GetSafeHwnd(),"startup fixture does not activate a user-desktop window");
            CRect client;frame->GetClientRect(&client);
            CImage image;image.Create(client.Width(),client.Height(),32);
            const auto dc=image.GetDC();frame->PrintWindow(CDC::FromHandle(dc),PW_CLIENTONLY);image.ReleaseDC();
            const wchar_t* capture=initial==0?L"startup_uart_default.png":initial==1?L"startup_uart_custom.png":
                initial==2?L"startup_i2c_default.png":L"startup_i2c_saved.png";
            check(SUCCEEDED(image.Save(capture)),"capture first restored-tab display");
            if(!uart) {
                auto& state=frame->session_state_;
                state.host_validated=true;frame->UpdateUi();
                check(frame->i2c_speed_combo_.GetCount()==0,"verified host without I2C ports does not inherit offline speeds");
                firmware_upgrade::bridge::Port live;
                live.id=0U;live.flags=1U;live.rates=1U;live.bus=firmware_upgrade::bridge::Bus::I2c;
                live.maximum_rx=live.maximum_tx=512U;live.label=L"IICA0";
                state.host_capabilities.feature_flags=firmware_upgrade::bridge::kFeatureI2c;
                state.i2c_ports={live};frame->UpdateUi();
                frame->i2c_speed_combo_.GetWindowTextW(value);
                check(frame->i2c_speed_combo_.GetCount()==1&&value==L"100 kHz","live100k-only capability replaces offline choices");
                state.i2c_ports[0].rates=2U;frame->UpdateUi();
                frame->i2c_speed_combo_.GetWindowTextW(value);
                check(frame->i2c_speed_combo_.GetCount()==1&&value==L"400 kHz","live400k-only capability remains authoritative");
                state.host_validated=false;state.i2c_ports.clear();frame->UpdateUi();
                frame->i2c_speed_combo_.GetWindowTextW(value);
                check(!frame->i2c_speed_combo_.IsWindowEnabled()&&frame->i2c_speed_combo_.GetCount()==2&&
                    value==(initial==3?L"400 kHz":L"100 kHz"),"disconnect restores disabled configured preview");
            }
            frame->worker_session_.StopAndJoin();frame->DestroyWindow();test_app.m_pMainWnd=nullptr;
        }
        UnhookWindowsHookEx(guard);
        check(WritePrivateProfileStringW(L"ui",L"bus_tab",L"CAN",ini.c_str())!=FALSE,"restore fixture CAN baseline");
        check(WritePrivateProfileStringW(L"ui",L"device_uart_baud",L"115200",ini.c_str())!=FALSE,"restore fixture baud baseline");
        check(WritePrivateProfileStringW(L"ui",L"i2c_speed",L"0",ini.c_str())!=FALSE,"restore fixture speed baseline");
        for (int scale : {100,125,150}) {
            auto* frame=new CMainFrame();
            if (!frame->Create(nullptr,L"Interface geometry - offline fixture",WS_OVERLAPPEDWINDOW,CRect(0,0,1366,900))) return 3;
            test_app.m_pMainWnd=frame;
            frame->worker_session_.StopAndJoin();
            while(frame->worker_session_.HasPendingEvents()) frame->worker_session_.DrainEvents();
            frame->worker_started_=false;frame->worker_command_pending_=false;
            LOGFONT font={};frame->ui_font_.GetLogFont(&font);
            font.lfHeight=MulDiv(font.lfHeight,scale,100);
            CFont replacement;if(!replacement.CreateFontIndirect(&font)) return 4;
            const auto old_font=frame->ui_font_.Detach();frame->ui_font_.Attach(replacement.Detach());
            struct FontSwap {HFONT before,after;} fonts{reinterpret_cast<HFONT>(old_font),reinterpret_cast<HFONT>(frame->ui_font_.GetSafeHandle())};
            EnumChildWindows(frame->GetSafeHwnd(),[](HWND child,LPARAM value)->BOOL {
                const auto& swap=*reinterpret_cast<const FontSwap*>(value);
                if(reinterpret_cast<HFONT>(::SendMessageW(child,WM_GETFONT,0,0))==swap.before)
                    ::SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(swap.after),FALSE);
                return TRUE;
            },reinterpret_cast<LPARAM>(&fonts));
            frame->build_identity_.SetFont(fonts.after);::DeleteObject(old_font);
            font.lfWeight=FW_BOLD;CFont bold;if(!bold.CreateFontIndirect(&font))return 5;
            frame->section_font_.DeleteObject();frame->section_font_.Attach(bold.Detach());
            auto& state=frame->session_state_;state={};
            state.host_validated=state.serial_open=true;
            state.host_capabilities.feature_flags=firmware_upgrade::bridge::kFeatureI2c|firmware_upgrade::bridge::kFeatureUart;
            firmware_upgrade::bridge::Port port;port.flags=1U;port.rates=3U;
            port.maximum_rx=port.maximum_tx=512U;port.bus=firmware_upgrade::bridge::Bus::I2c;port.label=L"IICA0 SCL62/SDA63";
            state.i2c_ports={port};port.bus=firmware_upgrade::bridge::Bus::Uart;port.rates=0x80000001U;port.label=L"UART1 TX74/RX75";state.uart_ports={port};
            frame->bus_tabs_.SetCurSel(2);
            frame->UpdateUi();
            const auto unselected=[&]() {
                const DWORD selection=frame->uart_baud_combo_.GetEditSel();
                return LOWORD(selection)==HIWORD(selection);
            };
            CString baud;
            frame->uart_baud_combo_.GetWindowTextW(baud);
            check(baud==L"115200","default UART baud remains 115200");
            frame->uart_baud_combo_.SetEditSel(0,-1);
            state.host_validated=state.serial_open=false;
            frame->UpdateUi();
            check(!frame->uart_baud_combo_.IsWindowEnabled()&&unselected(),"disconnected baud has no text highlight");
            frame->uart_baud_combo_.SetWindowTextW(L"123457");
            frame->uart_baud_combo_.SetEditSel(0,-1);
            frame->UpdateUi();frame->uart_baud_combo_.GetWindowTextW(baud);
            check(baud==L"123457"&&unselected(),"idle refresh preserves custom baud without selection");
            state.host_validated=state.serial_open=true;frame->UpdateUi();
            // Exercise the real notification dispatch without activating a user
            // desktop window. Physical focused editing remains a separate check.
            frame->uart_baud_combo_.SetEditSel(1,3);state.uart_valid=true;
            frame->SendMessageW(WM_COMMAND,MAKEWPARAM(ID_UART_BAUD_COMBO,CBN_KILLFOCUS),
                reinterpret_cast<LPARAM>(frame->uart_baud_combo_.GetSafeHwnd()));
            check(unselected()&&state.uart_valid,"focus-loss notification clears selection without invalidating discovery");
            frame->uart_baud_combo_.GetWindowTextW(baud);
            check(baud==L"123457","focus-loss notification preserves custom baud");
            frame->uart_baud_combo_.SetCurSel(4);state.host_validated=state.serial_open=false;state.uart_valid=false;
            state.uart_ports.clear();
            frame->UpdateUi();
            if(scale==100) {
                CRect idle_client;frame->GetClientRect(&idle_client);frame->LayoutControls(idle_client.Width(),idle_client.Height());
                frame->ShowWindow(SW_SHOWNOACTIVATE);frame->UpdateWindow();
                CImage idle;idle.Create(idle_client.Width(),idle_client.Height(),32);
                const auto dc=idle.GetDC();frame->PrintWindow(CDC::FromHandle(dc),PW_CLIENTONLY);idle.ReleaseDC();
                check(SUCCEEDED(idle.Save(L"uart_baud_idle.png")),"capture disconnected baud selection field");
                frame->ShowWindow(SW_HIDE);
            }
            state.host_validated=state.serial_open=true;state.uart_ports={port};
            for(int width : {1366,1920}) for(int route=0;route<7;++route) {
                ++cases;
                frame->bus_tabs_.SetCurSel(route<3?1:route<5?2:route==5?0:3);
                if(route<3)frame->i2c_workflow_combo_.SetCurSel(route==0?1:route==1?0:2);
                if(route>=3&&route<5)frame->uart_workflow_combo_.SetCurSel(route==3?0:3);
                frame->UpdateUi();
                MINMAXINFO minimum={};frame->SendMessageW(WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&minimum));
                CRect window,client;frame->GetWindowRect(&window);frame->GetClientRect(&client);
                frame->MoveWindow(0,0,width+window.Width()-client.Width(),(std::max)(768,static_cast<int>(minimum.ptMinTrackSize.y)),FALSE);
                frame->GetClientRect(&client);frame->LayoutControls(client.Width(),client.Height());
                const auto bounds=[&](CWnd& control) {CRect r;control.GetWindowRect(&r);frame->ScreenToClient(&r);return r;};
                std::vector<CWnd*> rows;
                if(route<3)rows={&frame->bus_port_combo_,&frame->i2c_speed_combo_,&frame->i2c_address_edit_,&frame->i2c_workflow_combo_,
                    route==2?static_cast<CWnd*>(&frame->i2c_probe_btn_):&frame->connect_target_btn_};
                else if(route<5)rows={&frame->bus_port_combo_,&frame->uart_baud_combo_,&frame->uart_workflow_combo_,&frame->connect_target_btn_};
                if(route<5) {
                    if(route!=2)rows.push_back(&frame->enter_bootloader_btn_);
                    rows.push_back(&frame->disconnect_btn_);rows.push_back(&frame->connection_details_btn_);rows.push_back(&frame->i2c_notes_);
                    for(std::size_t i=1U;i<rows.size();++i)
                        check(bounds(*rows[i]).top-bounds(*rows[i-1U]).bottom>=firmware_upgrade::console::InterfaceRowGap,
                            "native field/button/note rows retain a visible gap");
                    const auto notes=bounds(frame->i2c_notes_);const auto& pane=frame->console_layout_.interface_pane;
                    check(notes.bottom<=pane.y+pane.height,"notes stay within the interface group");
                    CClientDC dc(&frame->i2c_notes_);auto* previous=dc.SelectObject(&frame->ui_font_);
                    TEXTMETRIC metric={};dc.GetTextMetrics(&metric);CString text;frame->i2c_notes_.GetWindowTextW(text);
                    CRect measured(0,0,notes.Width()-16,0);dc.DrawTextW(text,measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
                    dc.SelectObject(previous);
                    if(measured.Height()+6>notes.Height())std::cerr<<"NOTE route="<<route<<" scale="<<scale<<" width="<<width
                        <<" required="<<measured.Height()+6<<" available="<<notes.Height()<<"\n";
                    check(measured.Height()+6<=notes.Height(),"complete connection reminder fits its native note rectangle");
                    const int old_step=(std::max)(22,static_cast<int>(metric.tmHeight)+4)+2;
                    if(bounds(frame->bus_port_combo_).Height()>old_step)++old_overlaps;
                    const auto label=bounds(frame->bus_port_label_),field=bounds(frame->bus_port_combo_);
                    check(abs(label.CenterPoint().y-field.CenterPoint().y)<=1,"label aligns with the closed selection field");
                    if(route>=3) {
                        CRect dropdown;frame->uart_baud_combo_.GetDroppedControlRect(&dropdown);
                        check(dropdown.Height()>bounds(frame->uart_baud_combo_).Height()+80,"editable baud combo retains its popup extent");
                    }
                    if((width==1366&&scale==100)||(width==1920&&scale==125)) {
                        if(route==0||route==3) {
                            frame->ShowWindow(SW_SHOWNOACTIVATE);
                            frame->RedrawWindow(nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
                            CImage picture;picture.Create(client.Width(),client.Height(),32);
                            const auto target=picture.GetDC();frame->PrintWindow(CDC::FromHandle(target),PW_CLIENTONLY);picture.ReleaseDC();
                            const auto name=route==0?(scale==100?L"i2c_rows_100.png":L"i2c_rows_125.png"):
                                (scale==100?L"uart_rows_100.png":L"uart_rows_125.png");
                            check(SUCCEEDED(picture.Save(name)),"capture actual I2C/UART controls");frame->ShowWindow(SW_HIDE);
                        }
                    }
                } else {
                    const auto last=bounds(frame->disconnect_btn_);const auto& pane=frame->console_layout_.interface_pane;
                    check(last.bottom<=pane.y+pane.height,"shared row spacing preserves CAN/Misc button bounds");
                }
            }
            frame->DestroyWindow();test_app.m_pMainWnd=nullptr;
        }
        std::cout<<"Interface layout: "<<failures<<" failures; "<<cases<<" cases; old nominal spacing overlaps="<<old_overlaps<<"; no COM\n";
        std::cout<<"NOT RUN actual focused-keyboard editing; nonactivating fixture checks idle selection and native focus-loss notification dispatch.\n";
        return failures?1:0;
    }
};
int wmain(){return CanPrerequisiteUiFixture::Run();}
