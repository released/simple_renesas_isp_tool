// Actual MFC header geometry; private settings and no COM operations.
#include <afxwin.h>
#include <afxcmn.h>
#include <atlimage.h>
#include <iostream>
#include "../src/main_frame.h"

CWinApp test_app;
struct CanPrerequisiteUiFixture {
    static int Run() {
        if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_HIDE) || !AfxInitRichEdit2()) return 2;
        unsigned failures = 0U;
        const auto check = [&](bool okay, const char* reason) {
            if (!okay) { ++failures; std::cerr << "FAIL " << reason << '\n'; }
        };
        for (int scale : {100, 125, 150}) {
            auto* frame = new CMainFrame();
            if (!frame->Create(nullptr, L"Header layout - offline fixture", WS_OVERLAPPEDWINDOW, CRect(0, 0, 1366, 768))) return 3;
            test_app.m_pMainWnd = frame;
            frame->worker_session_.StopAndJoin();
            while (frame->worker_session_.HasPendingEvents()) frame->worker_session_.DrainEvents();
            frame->worker_started_ = false;
            frame->worker_command_pending_ = false;
            LOGFONT font = {};
            frame->ui_font_.GetLogFont(&font);
            font.lfHeight = MulDiv(font.lfHeight, scale, 100);
            CFont replacement;
            if (!replacement.CreateFontIndirect(&font)) return 4;
            const auto old_font = frame->ui_font_.Detach();
            frame->ui_font_.Attach(replacement.Detach());
            struct FontSwap { HFONT before, after; } swap{reinterpret_cast<HFONT>(old_font), reinterpret_cast<HFONT>(frame->ui_font_.GetSafeHandle())};
            EnumChildWindows(frame->GetSafeHwnd(), [](HWND child, LPARAM value) -> BOOL {
                const auto& fonts = *reinterpret_cast<const FontSwap*>(value);
                if (reinterpret_cast<HFONT>(::SendMessageW(child, WM_GETFONT, 0, 0)) == fonts.before)
                    ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(fonts.after), FALSE);
                return TRUE;
            }, reinterpret_cast<LPARAM>(&swap));
            frame->build_identity_.SetFont(swap.after);
            ::DeleteObject(old_font);
            font.lfWeight = FW_BOLD;
            CFont bold;
            if (!bold.CreateFontIndirect(&font)) return 5;
            frame->section_font_.DeleteObject();
            frame->section_font_.Attach(bold.Detach());
            for (int width : {1260, 1366, 1920, 300}) {
                CRect window, client;
                frame->GetWindowRect(&window); frame->GetClientRect(&client);
                frame->MoveWindow(0, 0, width + window.Width() - client.Width(), 768, FALSE);
                frame->GetClientRect(&client);
                // A frame enforces its minimum tracking size; exercise the narrower
                // layout argument directly without changing that production minimum.
                frame->LayoutControls(width, client.Height());
                CRect version, com;
                ::GetWindowRect(frame->build_identity_.Handle(), &version);
                frame->ScreenToClient(&version);
                frame->com_combo_.GetWindowRect(&com); frame->ScreenToClient(&com);
                CClientDC dc(frame);
                auto* previous = dc.SelectObject(&frame->section_font_);
                const int title_width = dc.GetTextExtent(firmware_upgrade::console::HostConnectionTitle).cx;
                dc.SelectObject(previous);
                check(version.left >= 0 && version.right <= width, "build identity stays within client width");
                check(version.bottom <= com.top, "build identity never overlaps connection controls");
                if (width >= 1260) {
                    check(frame->console_layout_.connection_group.y <= 8, "normal widths do not reserve a blank header row");
                    check(version.left > frame->console_layout_.connection_group.x + title_width + 16, "version and Host title do not overlap");
                    check(version.Width() >= frame->build_identity_.PreferredWidth(), "normal widths display complete build identity");
                } else {
                    check(frame->console_layout_.connection_group.y >= version.bottom, "narrow fallback separates build identity and title");
                }
                if ((width == 1366 && scale == 100) || (width == 1920 && scale == 125)) {
                    frame->ShowWindow(SW_SHOWNOACTIVATE);
                    frame->RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                    CImage picture; picture.Create(client.Width(), client.Height(), 32);
                    const auto target = picture.GetDC();
                    frame->PrintWindow(CDC::FromHandle(target), PW_CLIENTONLY);
                    picture.ReleaseDC();
                    check(SUCCEEDED(picture.Save(width == 1366 ? L"header_1366_100.png" : L"header_1920_125.png")), "capture actual compact header");
                    frame->ShowWindow(SW_HIDE);
                }
            }
            frame->DestroyWindow(); test_app.m_pMainWnd = nullptr;
        }
        std::cout << "Header layout: " << failures << " failures; 12 width/font cases; no COM\n";
        return failures ? 1 : 0;
    }
};
int wmain() { return CanPrerequisiteUiFixture::Run(); }
