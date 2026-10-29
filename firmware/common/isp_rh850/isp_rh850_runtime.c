#include "r_smc_entry.h"
#include "custom_func.h"
#include "retarget.h"
#include "isp_rh850.h"
#include <string.h>
#if defined(ISP_XMODEM)
#include "isp_xmodem.h"
#endif
#if defined(ISP_DEVICE_APP)
#include "app_identity.h"
#include "app_runtime_log.h"
volatile uint32_t g_cg_sync_read;
#endif

static volatile unsigned long g_tick;
volatile UART_MANAGER_T UART0Manager;
static IspDevice g_device;
static IspStream g_stream;
static IspPacket g_request, g_reply;
static unsigned char g_wire[ISP_FRAME_MAX];
static unsigned short g_last_rx;
#if defined(ISP_DEVICE_BOOT)
static unsigned short g_last_activity;
#endif
static unsigned char g_pending;
#if defined(ISP_XMODEM)
static IspXmodem g_x;
static unsigned char g_x_active;
#if !defined(ISP_DEVICE_BOOT)
static unsigned long g_sink_crc;
#endif
#endif

static const IspDeviceConfig g_config = {
    ISP_CONTRACT_ID,
    0U,
    ISP_UPDATE_TRANSPORT,
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
    ISP_BUS_RATE,
    0UL,
    0UL,
    0UL,
    0U,
    0U,
    {0U, 0U},
    {'R', '7', 'F', '7', '0', '1', '6', '8', '4', 0, 0, 0, 0, 0, 0, 0},
    {'R', 'H', '8', '5', '0', '/', 'F', '1', 'K', 'M', 0, 0},
    IMAGE_IRQ_ABI};

unsigned short IspTick(void)
{
    return (unsigned short)g_tick;
}
void ostmr_1ms_IRQ(void)
{
    ++g_tick;
}
void tmr_1ms_IRQ(void)
{
}
void UARTx_Process(unsigned char byte)
{
    (void)byte;
}
void UARTx_ErrorCheckProcess(unsigned char byte)
{
    (void)byte;
}
/* Foreground-only diagnostic output used by retarget.c.
 * Preserve a finite wait when UART0 is busy or stopped; ISP data uses its own link.
 */
int SendChar(unsigned char value)
{
    unsigned long guard;
    guard = 100000UL;
    while ((RLN30.LST & _UART_TRANSMISSION_OPERATED) && guard)
    {
        guard--;
    }
    if (guard)
    {
        RLN30.LUTDR.UINT16 = value;
        return (int)value;
    }
    return -1;
}
void Device_RequestReset(unsigned long request)
{
    volatile unsigned long *cookie;
    unsigned char retry;
    DI();
    cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
    cookie[0] = BOOT_APP_SIGNATURE;
    cookie[1] = request;
    cookie[2] = ~request;
    cookie[3] = BOOT_APP_OWNER_BOOT;
    __syncp();
    for (retry = 0U; retry < 4U; ++retry)
    {
        WPROTR.PROTCMD0 = 0xA5U;
        RESCTL.SWRESA = 1UL;
        RESCTL.SWRESA = ~1UL;
        RESCTL.SWRESA = 1UL;
        __syncp();
    }
    for (;;)
    {
        NOP();
    }
}
static unsigned char Metadata(const unsigned long *words)
{
    return (unsigned char)(ImageMetadataValidate(words) == IMAGE_METADATA_OK);
}
#if defined(ISP_DEVICE_BOOT)
static unsigned char g_flash_ready;
static unsigned short g_page_used;
static unsigned long g_page_address;
static union
{
    unsigned long words[64];
    unsigned char bytes[256];
} g_page;
static unsigned char FlushPage(void)
{
    if (!g_page_used)
    {
        return 1U;
    }
    if (!Device_FlashWritePage(g_page_address, g_page.bytes))
    {
        return 0U;
    }
    g_page_used = 0U;
    return 1U;
}
static unsigned char Erase(unsigned long address)
{
    if (address == IMAGE_APPLICATION_START)
    {
        g_page_used = 0U;
    }
    if (!g_flash_ready)
    {
        g_flash_ready = Device_FlashInit();
    }
    return (unsigned char)(g_flash_ready && !g_page_used && Device_FlashErase(address));
}
static unsigned char Write(unsigned long address, const unsigned char *bytes)
{
    if (!g_page_used)
    {
        if (address & 255UL)
        {
            return 0U;
        }
        g_page_address = address;
        memset(g_page.bytes, 0xFF, sizeof(g_page.bytes));
    }
    if (address != g_page_address + g_page_used || g_page_used > 252U)
    {
        return 0U;
    }
    memcpy(g_page.bytes + g_page_used, bytes, 4U);
    g_page_used += 4U;
    return (unsigned char)(g_page_used < 256U || FlushPage());
}
static unsigned char Verify(unsigned long size, unsigned long crc)
{
    device_image_info_t info;
    unsigned char footer[4];
    if (!FlushPage() || !Device_ImageValidate(Device_FlashRead, &info) || info.size != size ||
        !Device_FlashRead(IMAGE_APPLICATION_START + size - 4UL, footer, 4U))
    {
        return 0U;
    }
    return (unsigned char)(Device_Read32(footer) == crc);
}
#else
static unsigned char Erase(unsigned long address)
{
    (void)address;
    return 0U;
}
static unsigned char Write(unsigned long address, const unsigned char *bytes)
{
    (void)address;
    (void)bytes;
    return 0U;
}
static unsigned char Verify(unsigned long size, unsigned long crc)
{
    (void)size;
    (void)crc;
    return 0U;
}
#endif
static const IspDeviceOps g_ops = {Metadata, Erase, Write, Device_FlashRead, Verify};
static void Action(void)
{
    unsigned char action;
    action = g_pending;
    g_pending = 0U;
    if (action == ISP_ACTION_ENTER)
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
    if (action == ISP_ACTION_RESET)
    {
        Device_RequestReset(BOOT_APP_UPDATE_REQUEST);
    }
}
#if defined(ISP_XMODEM)
static unsigned char StreamWrite(unsigned long at, const unsigned char *data, unsigned short size)
{
#if defined(ISP_DEVICE_BOOT)
    return (unsigned char)(IspDeviceWrite(&g_device, at, data, size) == ISP_OK);
#else
    (void)at;
    g_sink_crc = Device_Crc32Update(g_sink_crc, data, size);
    g_device.written += size;
    return 1U;
#endif
}
static unsigned char StreamFinish(void)
{
#if defined(ISP_DEVICE_BOOT)
    if (IspDeviceFinish(&g_device) != ISP_OK)
    {
        return 0U;
    }
    g_pending = g_device.action;
    return 1U;
#else
    g_device.active = 0U;
    return (unsigned char)((g_sink_crc ^ 0xFFFFFFFFUL) == g_device.image_crc);
#endif
}
static void StreamAbort(void)
{
    g_device.active = 0U;
    g_device.staged = 0U;
    g_pending = 0U;
}
static void StreamSend(unsigned char byte)
{
    if (!IspBusSend(&byte, 1U))
    {
        StreamAbort();
        g_x.active = 0U;
    }
}
static const IspXmodemOps g_x_ops = {StreamWrite, StreamFinish, StreamAbort, StreamSend};
#endif
static void RuntimePoll(void)
{
    unsigned char byte;
    unsigned short length;
    IspBusPoll();
    if (g_pending && IspBusReplyComplete())
    {
        Action();
    }
#if defined(ISP_XMODEM)
    if (g_x_active)
    {
        IspXmodemPoll(&g_x, IspTick(), &g_x_ops);
        while (IspBusGet(&byte))
        {
            IspXmodemFeed(&g_x, byte, IspTick(), &g_x_ops);
        }
        if (!g_x.active)
        {
            g_x_active = 0U;
            if (IspBusReplyComplete())
            {
                Action();
            }
            IspStreamInit(&g_stream);
        }
#if defined(ISP_DEVICE_BOOT)
        g_last_activity = IspTick();
#endif
        return;
    }
#endif
    if (g_stream.count && (unsigned short)(IspTick() - g_last_rx) >= ISP_INTERBYTE_TIMEOUT_MS)
    {
        g_stream.count = 0U;
    }
#if defined(ISP_I2C)
    while (IspBusGet(&byte))
    {
        if (g_stream.count < ISP_FRAME_MAX)
        {
            g_stream.bytes[g_stream.count++] = byte;
        }
    }
    if (g_stream.count)
    {
#else
    while (IspBusGet(&byte))
    {
#endif
        g_last_rx = IspTick();
#if defined(ISP_I2C)
        if (IspDecode(g_stream.bytes, g_stream.count, &g_request))
        {
#else
        if (IspStreamFeed(&g_stream, byte, &g_request))
        {
#endif
#if defined(ISP_DEVICE_BOOT)
            g_last_activity = g_last_rx;
#endif
            g_pending = 0U;
#if defined(ISP_DEVICE_APP)
            g_device.ready = (unsigned char)(IspTick() != 0U);
#endif
            IspDeviceProcess(&g_device, &g_request, &g_reply);
            length = IspEncode(&g_reply, g_wire);
            if (!IspBusSend(g_wire, length))
            {
                return;
            }
            if (!g_reply.status)
            {
#if defined(ISP_XMODEM)
                if (g_device.active && (g_request.command == ISP_CMD_START ||
                                        g_request.command == ISP_CMD_FORCE_START ||
                                        g_device.action == ISP_ACTION_STREAM_TEST))
                {
                    IspXmodemInit(&g_x, g_device.image_size, IspTick());
                    g_x_active = 1U;
#if !defined(ISP_DEVICE_BOOT)
                    g_sink_crc = 0xFFFFFFFFUL;
#endif
                    return;
                }
#endif
                g_pending = g_device.action;
                if (IspBusReplyComplete())
                {
                    Action();
                }
            }
        }
    }
#if defined(ISP_I2C)
    g_stream.count = 0U;
#endif
}
static unsigned char RuntimeStart(void)
{
    DI();
    g_tick = 0UL;
#if defined(ISP_DEVICE_APP)
    if (CLKCTL.PLL1C != 0x0001031DUL || !(CLKCTL.PLL1S & 4UL) || CLKCTL.CKSC_CPUCLKS_ACT != 3UL ||
        CLKCTL.CKSC_CPUCLKD_ACT != 0x11UL || CLKCTL.CKSC_PPLLCLKS_ACT != 3UL ||
        CLKCTL.CKSC_IPERI2S_ACT != 1UL)
    {
        return 0U;
    }
#endif
    R_Systeminit();
    R_Config_UART0_Start();
    /* EVM heartbeat GPIOs; do not rely on reset pin direction/latches. */
    PORT.PMC0 &= ~(1UL << 14);
    PORT.P0 |= (1UL << 14);
    PORT.PM0 &= ~(1UL << 14);
    PORT.PMC8 &= ~(1UL << 5);
    PORT.P8 |= (1UL << 5);
    PORT.PM8 &= ~(1UL << 5);
    INTC2.ICRLIN30UR0.BIT.MKRLIN30UR0 = 1U;
    INTC2.ICRLIN30UR1.BIT.MKRLIN30UR1 = 1U;
    INTC2.ICRLIN30UR2.BIT.MKRLIN30UR2 = 1U;
    R_Config_OSTM0_Start();
    if (!Device_ReadPrepare() || !IspBusInit())
    {
        return 0U;
    }
    EI();
    return 1U;
}
#if defined(ISP_DEVICE_BOOT)
static void Launch(unsigned long flags)
{
    volatile unsigned long *cookie;
    DI();
    IspBusStop();
    R_Config_OSTM0_Stop();
    R_Config_TAUJ0_0_Stop();
    R_Config_UART0_Stop();
    cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
    cookie[0] = BOOT_APP_SIGNATURE;
    cookie[1] = 0UL;
    cookie[2] = 0xFFFFFFFFUL;
    cookie[3] = BOOT_APP_OWNER_APP | flags;
    __syncp();
    Device_JumpAsm();
    for (;;)
    {
        NOP();
    }
}
#endif

#if !defined(ISP_DEVICE_TRANSFER)
static volatile unsigned long *g_custom_cookie;
#endif
static device_image_info_t g_custom_installed;
static unsigned long g_custom_last;
#if defined(ISP_DEVICE_BOOT)
static unsigned long g_custom_request;
#elif defined(ISP_DEVICE_APP)
static unsigned long g_custom_flags, g_custom_counter;
#endif

void IspProjectInit(void)
{
#if !defined(ISP_DEVICE_TRANSFER)
    g_custom_cookie = (volatile unsigned long *)BOOT_APP_SHARED_RAM_ADDRESS;
#endif
#if defined(ISP_DEVICE_BOOT)
    g_custom_request = 0UL;
    if (g_custom_cookie[0] == BOOT_APP_SIGNATURE && g_custom_cookie[2] == ~g_custom_cookie[1])
    {
        g_custom_request = g_custom_cookie[1];
    }
    g_custom_cookie[0] = BOOT_APP_SIGNATURE;
    g_custom_cookie[1] = 0UL;
    g_custom_cookie[2] = 0xFFFFFFFFUL;
    g_custom_cookie[3] = BOOT_APP_OWNER_BOOT;
#elif defined(ISP_DEVICE_APP)
    if (g_custom_cookie[0] != BOOT_APP_SIGNATURE || g_custom_cookie[1] ||
        g_custom_cookie[2] != 0xFFFFFFFFUL || (g_custom_cookie[3] & 255UL) != BOOT_APP_OWNER_APP)
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
    g_custom_flags = g_custom_cookie[3];
#endif
    if (!RuntimeStart())
    {
        for (;;)
        {
            NOP();
        }
    }
    memset(&g_custom_installed, 0, sizeof(g_custom_installed));
#if !defined(ISP_DEVICE_TRANSFER)
    (void)Device_ImageValidate(Device_FlashRead, &g_custom_installed);
#endif
#if defined(ISP_DEVICE_BOOT)
    if (g_custom_installed.valid && g_custom_request != BOOT_APP_ENTER_REQUEST)
    {
        Launch(g_custom_request == BOOT_APP_UPDATE_REQUEST ? BOOT_APP_UPDATED_FLAG : 0UL);
    }
    IspDeviceInit(&g_device, &g_config, &g_ops, ISP_MODE_BOOT);
#elif defined(ISP_DEVICE_APP)
    if (!g_custom_installed.valid || g_custom_installed.version != DEVICE_APP_VERSION ||
        g_custom_installed.build != DEVICE_APP_BUILD)
    {
        Device_RequestReset(BOOT_APP_ENTER_REQUEST);
    }
    IspDeviceInit(&g_device, &g_config, &g_ops, ISP_MODE_APP);
#if !defined(ISP_I2C)
    IspDeviceSetDiagnostics(&g_device, IspBusDiagnostics);
#endif
    tiny_printf("app image tag: " APP_RUNTIME_LOG_TAG "\r\n");
    if (g_custom_flags & BOOT_APP_UPDATED_FLAG)
    {
        tiny_printf("ISP update: completed\r\n");
    }
    g_custom_cookie[3] = BOOT_APP_OWNER_APP;
#else
    IspDeviceInit(&g_device, &g_config, &g_ops, ISP_MODE_TRANSFER);
#if !defined(ISP_I2C)
    IspDeviceSetDiagnostics(&g_device, IspBusDiagnostics);
#endif
#endif
    if (g_custom_installed.valid)
    {
        IspDeviceInstalled(&g_device, g_custom_installed.version, g_custom_installed.build,
                           g_custom_installed.size,
                           (unsigned char)g_custom_installed.version_control);
    }
    IspStreamInit(&g_stream);
    g_custom_last = g_tick;
#if defined(ISP_DEVICE_BOOT)
    g_last_activity = IspTick();
#elif defined(ISP_DEVICE_APP)
    g_custom_counter = 0UL;
#endif
    tiny_printf("RH850 serial ISP: EVM R7F701684; X1=16M CPU120\r\n");
}

void IspProjectPoll(void)
{

    RuntimePoll();
#if defined(ISP_DEVICE_BOOT)
    if (g_custom_installed.valid && !g_device.dirty && !g_device.active &&
        (unsigned short)(IspTick() - g_last_activity) >= ISP_BOOT_IDLE_TIMEOUT_MS)
    {
        if (Device_ImageValidate(Device_FlashRead, &g_custom_installed))
        {
            Launch(0UL);
        }
    }
#endif
    if (g_tick - g_custom_last >= 1000UL)
    {
        g_custom_last = g_tick;
        PORT.P0 ^= (1UL << 14);
        PORT.P8 ^= (1UL << 5);
#if defined(ISP_DEVICE_APP)
        tiny_printf("log(timer)-" APP_RUNTIME_LOG_TAG ": %lu\r\n", g_custom_counter++);
#endif
    }
}
