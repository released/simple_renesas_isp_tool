#include "platform.h"
#include "isp_rl78_board.h"

#if !defined(ISP_DEVICE_APP)
#if defined(ISP_MCU_F24)
extern void Device_Jump(void);
#endif
#if defined(ISP_SMALL_TARGET) && !defined(ISP_DEVICE_APP)
void boot_main(void)
#else
void main(void)
#endif
{
#if defined(ISP_DEVICE_BOOT)
    unsigned long marker;
#endif
#if defined(ISP_DEVICE_BOOT)
    marker = *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL);
#endif
#if !defined(ISP_DEVICE_TRANSFER)
    *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL) = 0UL;
#endif
    if (!IspBoardInit()) for (;;) { }
#if defined(ISP_DEVICE_TRANSFER)
    IspRuntimeInit(ISP_MODE_TRANSFER);
    IspLog("ISP UART Custom Transfer: binary UART1; diagnostic UART0\r\n");
    for (;;) { IspRuntimePoll(); }
#else
    if (IspInstalledValid() && marker != BOOT_APP_RESET_TO_BOOT_SIGNATURE) {
        IspRuntimeHandoff();
#if defined(ISP_SMALL_TARGET)
        return;
#else
        Device_Jump();
#endif
    }
    IspRuntimeInit(ISP_MODE_BOOT);
    for (;;) {
        IspRuntimePoll();
        if (marker == BOOT_APP_RESET_TO_BOOT_SIGNATURE && !IspRuntimeActive() &&
            IspRuntimeIdle() >= ISP_BOOT_IDLE_TIMEOUT_MS && IspInstalledValid()) {
            IspRuntimeHandoff();
#if defined(ISP_SMALL_TARGET)
            return;
#else
            Device_Jump();
#endif
        }
    }
#endif
}
#endif
