/* Included after the board-specific Flash/metadata adapter. This is main-loop
 * code, not an ISR. Each project selects exactly one update protocol.
 */
static unsigned char g_pending_action;
#if defined(ISP_XMODEM)
static unsigned char g_x_active;
#if !defined(ISP_DEVICE_BOOT)
static unsigned long g_x_crc;
#endif
unsigned char IspXmodemWrite(unsigned long offset, const unsigned char ISP_RAM *data, unsigned short size)
{
#if !defined(ISP_DEVICE_BOOT)
    unsigned short at;
    unsigned char bit;
    (void)offset;
    for (at = 0U; at < size; ++at) {
        g_x_crc ^= data[at];
        for (bit = 0U; bit < 8U; ++bit)
            g_x_crc = (g_x_crc & 1UL) ? (g_x_crc >> 1) ^ 0xEDB88320UL : g_x_crc >> 1;
    }
    g_device.written += size;
    return 1U;
#else
    return (unsigned char)(IspDeviceWrite(&g_device, offset, data, size) == ISP_OK);
#endif
}
unsigned char IspXmodemFinish(void)
{
#if defined(ISP_DEVICE_BOOT)
    if (IspDeviceFinish(&g_device) != ISP_OK) return 0U;
    g_pending_action = g_device.action;
    return 1U;
#else
    g_device.active = 0U; return (unsigned char)((g_x_crc ^ 0xFFFFFFFFUL) == g_device.image_crc);
#endif
}
void IspXmodemAbort(void) { g_device.active = 0U; g_device.staged = 0U; g_pending_action = 0U; }
void IspXmodemSend(unsigned char byte)
{
    /* A local TX failure is not a lost ACK at the peer. Do not retain a
     * successful EOT's reset action when its final ACK could not leave TX.
     */
    if (!IspUartSend(&byte, 1U)) {
        IspXmodemAbort(); g_input.x.active = 0U;
    }
}
#endif
static void perform_action(void)
{
#if defined(ISP_DEVICE_APP)
    if (g_pending_action == ISP_ACTION_ENTER)
        *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 12UL) = BOOT_APP_RESET_TO_BOOT_SIGNATURE;
#else
    if (g_pending_action == ISP_ACTION_RESET)
        *(volatile unsigned long __far *)(BOOT_APP_SHARED_RAM_ADDRESS + 8UL) = BOOT_APP_UPDATE_COMPLETE_SIGNATURE;
#endif
    if (g_pending_action) {
        g_pending_action = 0U;
        IspBoardReset();
    }
}
void IspRuntimePoll(void)
{
    unsigned char byte;
    unsigned short length;
#if defined(ISP_I2C)
    IspI2cPoll(IspTick());
    if (g_pending_action && IspI2cReplyComplete()) perform_action();
#elif defined(ISP_XMODEM)
    if (g_x_active) {
        IspXmodemPoll(&g_input.x, IspTick(), 0);
        while (IspUartGet(&byte)) IspXmodemFeed(&g_input.x, byte, IspTick(), 0);
        if (!g_input.x.active) {
            g_x_active = 0U; perform_action(); IspStreamInit(&g_stream);
        }
        g_last_activity = IspTick(); IspHeartbeat(); return;
    }
#endif
    if (g_stream.count && (unsigned short)(IspTick() - g_last_rx) >= ISP_INTERBYTE_TIMEOUT_MS)
        g_stream.count = 0U;
#if defined(ISP_I2C)
    while (IspI2cGet(&byte)) {
        if (g_stream.count < ISP_FRAME_MAX) g_stream.bytes[g_stream.count++] = byte;
    }
    if (g_stream.count) {
#else
    while (IspUartGet(&byte)) {
#endif
        g_last_rx = IspTick();
#if defined(ISP_I2C)
        if (IspDecode(g_stream.bytes, g_stream.count, &g_request)) {
#else
        if (IspStreamFeed(&g_stream, byte, &g_request)) {
#endif
            g_last_activity = g_last_rx; g_pending_action = 0U;
#if defined(ISP_DEVICE_APP)
            if (IspTick() != 0U) g_device.ready = 1U;
#endif
            IspDeviceProcess(&g_device, &g_request, &g_reply);
            length = IspEncode(&g_reply, g_wire);
#if defined(ISP_I2C)
            if (!IspI2cSend(g_wire, length)) return;
            if (!g_reply.status) g_pending_action = g_device.action;
#else
            if (!IspUartSend(g_wire, length)) return;
            if (!g_reply.status) {
#if defined(ISP_XMODEM)
                if (((g_request.command == ISP_CMD_START || g_request.command == ISP_CMD_FORCE_START) && g_device.active) ||
                    g_device.action == ISP_ACTION_STREAM_TEST) {
                    IspXmodemInit(&g_input.x, g_device.image_size, IspTick()); g_x_active = 1U;
#if !defined(ISP_DEVICE_BOOT)
                    g_x_crc = 0xFFFFFFFFUL;
#endif
                    return;
                }
#endif
                g_pending_action = g_device.action; perform_action();
            }
#endif
        }
    }
#if defined(ISP_I2C)
    IspI2cRelease();
    /* I2C requests have a STOP boundary; a partial frame cannot consume bytes
     * from a later transaction or block a valid subsequent request. */
    g_stream.count = 0U;
#endif
    IspHeartbeat();
}
