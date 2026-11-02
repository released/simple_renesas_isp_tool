#include "app.h"

#include "build_info.generated.h"
#include "main_frame.h"

#include <string>

BEGIN_MESSAGE_MAP(CFirmwareUpgradeToolApp, CWinApp)
END_MESSAGE_MAP()

BOOL CFirmwareUpgradeToolApp::InitInstance() {
    namespace instance = firmware_upgrade::application_instance;
    std::wstring error;
    const auto ownership = instance_.Acquire(&error);
    if (ownership != instance::StartResult::Owner) {
        if (ownership == instance::StartResult::Failed || !instance_.NotifyExisting(5000U, &error)) {
            instance::ShowStartupFailure(error);
        }
        return FALSE;
    }
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
        // CFrameWnd::PostNcDestroy owns deletion, including failed Create.
        return FALSE;
    }

    if (!instance_.Attach(frame->GetSafeHwnd())) {
        frame->DestroyWindow();
        instance::ShowStartupFailure(L"Could not register the Tool window. Startup was stopped.");
        return FALSE;
    }
    m_pMainWnd = frame;
    frame->ShowWindow(SW_SHOWMAXIMIZED);
    frame->UpdateWindow();
    return TRUE;
}

int CFirmwareUpgradeToolApp::ExitInstance() {
    const int result = CWinApp::ExitInstance();
    instance_.Release();
    return result;
}
