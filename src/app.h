#pragma once

#include <afxwin.h>
#include "core/application_instance.h"

class CFirmwareUpgradeToolApp : public CWinApp {
public:
    BOOL InitInstance() override;
    int ExitInstance() override;
private:
    firmware_upgrade::application_instance::Guard instance_;

    DECLARE_MESSAGE_MAP()
};
