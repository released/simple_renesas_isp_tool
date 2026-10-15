#include "app.h"

#include "build_info.generated.h"
#include "main_frame.h"

#include <string>

BEGIN_MESSAGE_MAP(CFirmwareUpgradeToolApp, CWinApp)
END_MESSAGE_MAP()

BOOL CFirmwareUpgradeToolApp::InitInstance() {
    CWinApp::InitInstance();

    if (!AfxInitRichEdit2()) {
        return FALSE;
    }

    const std::wstring title = std::wstring(L"Firmware Upgrade Tool v") + FIRMWARE_UPGRADE_TOOL_VERSION;
    auto* frame = new CMainFrame();
    if (!frame->Create(nullptr,
                       title.c_str(),
                       WS_OVERLAPPEDWINDOW,
                       CRect(0, 0, 1366, 728))) {
        delete frame;
        return FALSE;
    }

    m_pMainWnd = frame;
    frame->ShowWindow(SW_SHOWMAXIMIZED);
    frame->UpdateWindow();
    return TRUE;
}
