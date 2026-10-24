/* Compile the production main-loop transport, mocking only board byte I/O,
 * tick, heartbeat and reset. The protocol/receiver/CRC sink are not mocks.
 */
#include "isp_xmodem_runtime_harness.h"
#include "isp_xmodem.h"
#include <stdint.h>
#include <string.h>

#if defined(ISP_DEVICE_BOOT)
#define g_device g_isp_device
#define g_request g_isp_request
#define g_reply g_isp_reply
#else
static IspDevice g_device;
static IspPacket g_request, g_reply;
#endif
#define g_input g_isp_input
#define g_stream g_input.stream
static unsigned char g_wire[ISP_FRAME_MAX];
static unsigned short g_last_rx, g_last_activity, test_tick;
static unsigned char test_rx[512], test_tx[512];
static unsigned short test_rx_size, test_rx_at, test_tx_size;
static unsigned long test_resets, test_heartbeats, test_handoff[4];
static unsigned char test_send_ok;
#define __far
#define BOOT_APP_SHARED_RAM_ADDRESS ((uintptr_t)test_handoff)
#define BOOT_APP_RESET_TO_BOOT_SIGNATURE 0x12345678UL
#define BOOT_APP_UPDATE_COMPLETE_SIGNATURE 0x23456789UL
#define ISP_INTERBYTE_TIMEOUT_MS 200U
static unsigned short IspTick(void) { return test_tick; }
static void IspHeartbeat(void) { ++test_heartbeats; }
static void IspBoardReset(void) { ++test_resets; }
static unsigned char IspUartGet(unsigned char *byte)
{
    if (test_rx_at == test_rx_size) return 0U;
    *byte = test_rx[test_rx_at++]; return 1U;
}
static unsigned char IspUartSend(const unsigned char *bytes, unsigned short size)
{
    if (!test_send_ok || size > sizeof(test_tx) - test_tx_size) return 0U;
    memcpy(test_tx + test_tx_size, bytes, size); test_tx_size += size; return 1U;
}
#include "../firmware/common/isp_device/isp_runtime_transport.inc"

void IspRuntimeTestInit(const IspDeviceConfig *config, const IspDeviceOps *ops)
{
#if defined(ISP_DEVICE_BOOT)
    IspDeviceInit(&g_device, config, ops, ISP_MODE_BOOT);
#elif defined(ISP_DEVICE_APP)
    IspDeviceInit(&g_device, config, ops, ISP_MODE_APP);
    IspDeviceInstalled(&g_device, 0x00010600UL, 20261023UL, 516UL, 0U);
#else
    IspDeviceInit(&g_device, config, ops, ISP_MODE_TRANSFER);
#endif
    memset(&g_input, 0, sizeof(g_input)); IspStreamInit(&g_stream);
    memset(&g_request, 0, sizeof(g_request)); memset(&g_reply, 0, sizeof(g_reply));
    test_rx_size = test_rx_at = test_tx_size = test_tick = 0U;
    g_last_rx = g_last_activity = 0U; g_x_active = g_pending_action = 0U;
#if !defined(ISP_DEVICE_BOOT)
    g_x_crc = 0UL;
#endif
    test_resets = test_heartbeats = 0UL; test_send_ok = 1U;
    memset(test_handoff, 0, sizeof(test_handoff));
}
void IspRuntimeTestSendResult(unsigned char okay) { test_send_ok = okay; }
unsigned short IspRuntimeTestExchange(const unsigned char *input,
    unsigned short size, unsigned short advance, unsigned char *output)
{
    if (size > sizeof(test_rx) || test_rx_at != test_rx_size) return 0U;
    if (size) memcpy(test_rx, input, size);
    test_rx_size = size; test_rx_at = test_tx_size = 0U;
    test_tick = (unsigned short)(test_tick + advance); IspRuntimePoll();
    if (test_tx_size) memcpy(output, test_tx, test_tx_size);
    return test_tx_size;
}
IspRuntimeSnapshot IspRuntimeTestSnapshot(void)
{
    IspRuntimeSnapshot snapshot;
    snapshot.written = g_device.written; snapshot.resets = test_resets;
    snapshot.heartbeats = test_heartbeats; snapshot.active = g_device.active;
    snapshot.streaming = g_x_active; snapshot.pending = g_pending_action;
    snapshot.ready = g_device.ready; return snapshot;
}
