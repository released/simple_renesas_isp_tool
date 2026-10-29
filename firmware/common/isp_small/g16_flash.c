/* Project/API reading notes: MCU-specific erase/program adapter for the Boot update engine.
 * App partition and erase/page alignment checks protect Boot; library preparation and RAM
 * execution follow this MCU's Flash implementation.
 * Success means the requested library operation completed.
 */
#include "platform.h"
#include "user_def.h"
#include "isp_rl78_board.h"
#include "r_rfsp_common_api.h"
#include "r_rfsp_common_control_api.h"
#include "r_rfsp_code_flash_api.h"

/* RFSP Type01 is the G16 vendor implementation. The CPU stalls while its
 * sequencer rewrites Flash; interrupts are disabled around each operation.
 * Unlike the RFD RAM-running implementation, this API does not copy a library
 * into the device's 2-KiB RAM. See G16 hardware manual section22.6.
 */
static unsigned char wait_end(void)
{
    unsigned long budget;
    unsigned char error;
    budget = 2000000UL;
    while (R_RFSP_CheckCFDFSeqEndStep1() == R_RFSP_ENUM_RET_STS_BUSY)
    {
        if (!--budget)
        {
            IspBoardReset();
        }
    }
    budget = 2000000UL;
    while (R_RFSP_CheckCFDFSeqEndStep2() == R_RFSP_ENUM_RET_STS_BUSY)
    {
        if (!--budget)
        {
            IspBoardReset();
        }
    }
    R_RFSP_GetSeqErrorStatus(&error);
    return (unsigned char)(error == 0U);
}
static unsigned char execute(unsigned long address, const unsigned char *data, unsigned char erase)
{
    unsigned char result;
    result = 0U;
    if (R_RFSP_SetFlashMemoryMode(R_RFSP_ENUM_FLASH_MODE_CODE_PROGRAMMING) ==
        R_RFSP_ENUM_RET_STS_OK)
    {
        if (erase)
        {
            R_RFSP_EraseCodeFlashReq((unsigned char)(address >> 10));
        }
        else
        {
            R_RFSP_WriteCodeFlashReq((unsigned short)address, (unsigned char __near *)data);
        }
        result = wait_end();
    }
    if (R_RFSP_SetFlashMemoryMode(R_RFSP_ENUM_FLASH_MODE_NONPROGRAMMABLE) != R_RFSP_ENUM_RET_STS_OK)
    {
        IspBoardReset();
    }
    return result ? 0U : 1U;
}
/* Prepare the vendor Flash execution environment and verify device geometry before allowing an
 * update session to erase/program.
 */
unsigned char Device_FlashInit(void)
{
    return R_RFSP_Init(16U) == R_RFSP_ENUM_RET_STS_OK ? 0U : 1U;
}
unsigned char Device_FlashErase(unsigned long address)
{
    if (address < IMAGE_APPLICATION_START ||
        address >= IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY ||
        address % IMAGE_ERASE_BLOCK_SIZE)
    {
        return 1U;
    }
    return execute(address, 0, 1U);
}
unsigned char Device_FlashWrite(unsigned long address, unsigned char __near *data)
{
    if (address < IMAGE_APPLICATION_START ||
        address > IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY - 4UL || address % 4UL)
    {
        return 1U;
    }
    return execute(address, data, 0U);
}
void __far R_RFSP_HOOK_EnterCriticalSection(void)
{
    BSP_DI();
}
void __far R_RFSP_HOOK_ExitCriticalSection(void)
{ /* The runtime restores IE after Flash has returned. */
}
