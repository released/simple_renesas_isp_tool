#if defined(ISP_MCU_F13)
#include "r_cg_macrodriver.h"
#else
#include "platform.h"
#endif
#include "isp_rl78_board.h"
#include "custom_func.h"

#if defined(ISP_MCU_F24)
extern void Device_Jump(void);
#endif

#if defined(ISP_DEVICE_BOOT)
static unsigned long g_boot_marker;
static unsigned char g_boot_running;

unsigned char custom_func_active(void)
{
    return g_boot_running;
}
#endif

void hardware_init(void)
{
#if defined(ISP_DEVICE_BOOT)
    g_boot_running = 1U;
    g_boot_marker = *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL);
    *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL) = 0UL;
#endif
    if (!IspBoardInit())
    {
        for (;;)
        {
        }
    }
#if defined(ISP_DEVICE_TRANSFER)
    IspRuntimeInit(ISP_MODE_TRANSFER);
    IspLog("ISP BUS Custom Transfer: binary bus; diagnostic UART0\r\n");
#else
    if (IspInstalledValid() && g_boot_marker != BOOT_APP_RESET_TO_BOOT_SIGNATURE)
    {
        IspRuntimeHandoff();
#if defined(ISP_MCU_F24)
        Device_Jump();
#else
        g_boot_running = 0U;
        return;
#endif
    }
    IspRuntimeInit(ISP_MODE_BOOT);
#endif
}

void loop(void)
{
    IspRuntimePoll();
#if defined(ISP_DEVICE_BOOT)
    if (g_boot_marker == BOOT_APP_RESET_TO_BOOT_SIGNATURE && !IspRuntimeActive() &&
        IspRuntimeIdle() >= ISP_BOOT_IDLE_TIMEOUT_MS && IspInstalledValid())
    {
        IspRuntimeHandoff();
#if defined(ISP_MCU_F24)
        Device_Jump();
#else
        g_boot_running = 0U;
#endif
    }
#endif
}
