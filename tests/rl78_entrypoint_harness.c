/* Execute the actual project callbacks with mocked board boundaries. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "isp_rl78_board.h"
#include "custom_func.h"

unsigned long mock_ram[4];
static unsigned short tick;
static unsigned char installed, mode, inits, polls, handoffs, resets;
static jmp_buf jump_target;

unsigned char IspBoardInit(void)
{
    inits++;
    return 1U;
}

void IspBoardReset(void)
{
    resets++;
    longjmp(jump_target, 1);
}

void Device_Jump(void)
{
    longjmp(jump_target, 2);
}

unsigned char IspInstalledValid(void)
{
    return installed;
}

void IspRuntimeInit(unsigned char value)
{
    mode = value;
}

void IspRuntimePoll(void)
{
    polls++;
}

void IspRuntimeHandoff(void)
{
    handoffs++;
}

unsigned char IspRuntimeActive(void)
{
    return 0U;
}

unsigned short IspRuntimeIdle(void)
{
    return tick;
}

unsigned short IspTick(void)
{
    return tick;
}

void IspLog(const char *text)
{
    assert(text != NULL);
}

int main(void)
{
    memset(mock_ram, 0, sizeof(mock_ram));
#if defined(ISP_DEVICE_APP)
    if (setjmp(jump_target) == 0)
    {
        hardware_init();
        assert(0);
    }
    assert(resets == 1U);
    mock_ram[1] = BOOT_APP_HANDOFF_SIGNATURE;
    mock_ram[2] = BOOT_APP_UPDATE_COMPLETE_SIGNATURE;
    hardware_init();
    assert(mode == ISP_MODE_APP && inits == 1U);
    assert(mock_ram[1] == 0UL && mock_ram[2] == 0UL);
    tick = 1000U;
    loop();
    loop();
    assert(polls == 2U);
#elif defined(ISP_DEVICE_TRANSFER)
    hardware_init();
    assert(mode == ISP_MODE_TRANSFER && inits == 1U);
    loop();
    loop();
    assert(polls == 2U && handoffs == 0U);
#else
    installed = 0U;
    hardware_init();
    assert(mode == ISP_MODE_BOOT && custom_func_active());
    loop();
    assert(polls == 1U && handoffs == 0U);
    installed = 1U;
    mock_ram[3] = BOOT_APP_RESET_TO_BOOT_SIGNATURE;
    hardware_init();
    assert(mock_ram[3] == 0UL && custom_func_active());
    tick = ISP_BOOT_IDLE_TIMEOUT_MS;
    if (setjmp(jump_target) == 0)
    {
        loop();
    }
    assert(handoffs == 1U);
#if !defined(ISP_MCU_F24)
    assert(!custom_func_active());
#endif
    tick = 0U;
    mock_ram[3] = 0UL;
    if (setjmp(jump_target) == 0)
    {
        hardware_init();
    }
    assert(handoffs == 2U);
#endif
    puts("PASS actual custom_func initialization/foreground/handoff callbacks");
    return 0;
}
