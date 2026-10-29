#if defined(ISP_MCU_F13)
#include "r_cg_macrodriver.h"
#if defined(ISP_DEVICE_BOOT)
#include "fsl_flash_interface.h"
#endif
#else
#include "platform.h"
#include "user_def.h"
#define DI() BSP_DI()
#define EI() BSP_EI()
#endif
#include "isp_rl78_board.h"
#include "sw_crc32.h"

static IspDevice g_device;
static IspStream g_stream;
static IspPacket g_request, g_reply;
static unsigned char g_wire[ISP_FRAME_MAX];
static unsigned short g_last_rx;
static unsigned short g_last_activity;
static const IspDeviceConfig g_config = {
    1U,
    0U,
    4U,
    64U,
    IMAGE_TARGET_ID,
    0U,
    0U,
    IMAGE_APPLICATION_START,
    IMAGE_APPLICATION_CAPACITY,
    IMAGE_CODE_FLASH_SIZE,
    IMAGE_RESERVED_SIZE,
    IMAGE_ERASE_BLOCK_SIZE,
    IMAGE_BOOT_VERSION,
    IMAGE_BOOT_BUILD,
    ISP_UART_BAUD,
    0UL,
    0UL,
    0UL,
    0U,
    0U,
    {0U, 0U},
#if defined(ISP_MCU_F13)
    {'R', '5', 'F', '1', '0', 'B', 'M', 'G', 0, 0, 0, 0, 0, 0, 0, 0},
    {'R', 'L', '7', '8', '/', 'F', '1', '3', 0, 0, 0, 0},
#else
    {'R', '7', 'F', '1', '2', '4', 'F', 'P', 'J', '5', 'x', 'F', 'B', 0, 0, 0},
    {'R', 'L', '7', '8', '/', 'F', '2', '4', 0, 0, 0, 0},
#endif
    IMAGE_IRQ_ABI};
#if !defined(ISP_DEVICE_TRANSFER)
unsigned char IspMetadataCheck(const unsigned long *words)
{
    return (unsigned char)(ImageMetadataValidate(words) == IMAGE_METADATA_OK);
}
unsigned char IspRomRead(unsigned long address, unsigned char *data, unsigned short size)
{
    unsigned short i;
    if (address < IMAGE_APPLICATION_START ||
        address > IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY ||
        size > IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY - address)
    {
        return 0U;
    }
    for (i = 0U; i < size; ++i)
    {
        data[i] = *(const unsigned char __far *)(address + (unsigned long)i);
    }
    return 1U;
}
unsigned char IspInstalledValid(void)
{
    const unsigned long __far *words;
    const unsigned char __far *entry;
    unsigned long size, at, target;
    words = (const unsigned long __far *)(IMAGE_APPLICATION_START + 256UL);
    if (ImageMetadataValidate(words) != IMAGE_METADATA_OK)
    {
        return 0U;
    }
    size = words[6];
    if (caculate_crc32_checksum(IMAGE_APPLICATION_START, size - 4UL) !=
        *(const unsigned long __far *)(IMAGE_APPLICATION_START + size - 4UL))
    {
        return 0U;
    }
    for (at = 0UL; at < 256UL; at += 4UL)
    {
#if defined(ISP_MCU_F13)
        if (at == 4UL)
        {
            continue;
        }
#endif
        entry = (const unsigned char __far *)(IMAGE_APPLICATION_START + at);
        if (entry[0] != 0xECU)
        {
            return 0U;
        }
        target = (unsigned long)entry[1] | ((unsigned long)entry[2] << 8) |
                 ((unsigned long)entry[3] << 16);
        if (target < IMAGE_APPLICATION_START + 512UL ||
            target >= IMAGE_APPLICATION_START + size - 4UL)
        {
            return 0U;
        }
    }
    return 1U;
}
#if defined(ISP_DEVICE_BOOT)
static unsigned char g_flash_ready;
static unsigned char prepare_flash(void)
{
    if (g_flash_ready)
    {
        return 1U;
    }
#if defined(ISP_MCU_F13)
    g_flash_ready = (unsigned char)(init_fcl() == 0U);
#else
    g_flash_ready = (unsigned char)(Device_FlashInit() == 0U);
#endif
    return g_flash_ready;
}
unsigned char IspFlashErase(unsigned long address)
{
    unsigned char result;
    if (!prepare_flash())
    {
        return 0U;
    }
    DI();
#if defined(ISP_MCU_F13)
    result = (unsigned char)(flash_erase_app_area(address) == 0U);
#else
    result = (unsigned char)(Device_FlashErase(address) == 0U);
#endif
    EI();
    return result;
}
unsigned char IspFlashWrite(unsigned long address, const unsigned char *data)
{
    unsigned char result;
    /* The request buffer is data RAM and remains owned by the main loop until
     * the synchronous Flash call returns. The library never writes this input.
     */
    DI();
#if defined(ISP_MCU_F13)
    result =
        (unsigned char)(flash_write_packet_data(address, (unsigned char __near *)data, 4U) == 0U);
#else
    result = (unsigned char)(Device_FlashWrite(address, (unsigned char __near *)data) == 0U);
#endif
    EI();
    return result;
}
#endif
unsigned char IspImageVerify(unsigned long size, unsigned long crc)
{
    if (*(const unsigned long __far *)(IMAGE_APPLICATION_START + 256UL + 24UL) != size ||
        *(const unsigned long __far *)(IMAGE_APPLICATION_START + size - 4UL) != crc)
    {
        return 0U;
    }
    return IspInstalledValid();
}
#endif
void IspRuntimeInit(unsigned char mode)
{
#if !defined(ISP_DEVICE_TRANSFER)
    const unsigned long __far *words;
#endif
    IspStreamInit(&g_stream);
    IspDeviceInit(&g_device, &g_config, 0, mode);
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
    IspDeviceSetDiagnostics(&g_device, IspUartDiagnostics);
#endif
#if !defined(ISP_DEVICE_TRANSFER)
    if (IspInstalledValid())
    {
        words = (const unsigned long __far *)(IMAGE_APPLICATION_START + 256UL);
        IspDeviceInstalled(&g_device, words[4], words[5], words[6], (unsigned char)words[29]);
    }
#endif
    g_last_rx = IspTick();
    g_last_activity = g_last_rx;
}
unsigned char IspRuntimeActive(void)
{
    return g_device.active;
}
unsigned short IspRuntimeIdle(void)
{
    return (unsigned short)(IspTick() - g_last_activity);
}
void IspRuntimeHandoff(void)
{
    DI();
    IspUartStop();
    IspTimerStop();
#if defined(ISP_MCU_F13) && defined(ISP_DEVICE_BOOT)
    if (g_flash_ready)
    {
        FSL_Close();
    }
#endif
    *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 4UL) =
        BOOT_APP_HANDOFF_SIGNATURE;
    *(volatile unsigned char __far *)BOOT_APP_SHARED_RAM_ADDRESS = BOOT_APP_RAM_OWNER_APPLICATION;
}
void IspRuntimePoll(void)
{
    unsigned char byte;
    unsigned short length;
    if (g_stream.count && (unsigned short)(IspTick() - g_last_rx) >= ISP_INTERBYTE_TIMEOUT_MS)
    {
        g_stream.count = 0U;
    }
    while (IspUartGet(&byte))
    {
        g_last_rx = IspTick();
        if (IspStreamFeed(&g_stream, byte, &g_request))
        {
            g_last_activity = g_last_rx;
#if defined(ISP_DEVICE_APP)
            if (IspTick() != 0U)
            {
                g_device.ready = 1U;
            }
#endif
            IspDeviceProcess(&g_device, &g_request, &g_reply);
            length = IspEncode(&g_reply, g_wire);
            if (!IspUartSend(g_wire, length))
            {
                return;
            }
#if defined(ISP_DEVICE_APP)
            if (g_device.action == ISP_ACTION_ENTER)
            {
                *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL) =
                    BOOT_APP_RESET_TO_BOOT_SIGNATURE;
            }
#else
            if (g_device.action == ISP_ACTION_RESET)
            {
                *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 8UL) =
                    BOOT_APP_UPDATE_COMPLETE_SIGNATURE;
            }
#endif
            if (g_device.action)
            {
                IspBoardReset();
            }
        }
    }
    IspHeartbeat();
}
