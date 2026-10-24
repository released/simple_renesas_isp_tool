#include "isp_device.h"
#include "isp_fixed_profile.h"
#include <string.h>
#if defined(ISP_DEVICE_BOOT) && defined(ISP_XMODEM) && \
    (defined(__CCRL__) || defined(ISP_TEST_DEVICE_CONTEXT))
IspDevice g_isp_device;
IspPacket g_isp_request, g_isp_reply;
#define DEVICE_CONTEXT(state) ((state) = &g_isp_device)
#define REQUEST_CONTEXT(packet) ((packet) = &g_isp_request)
#define REPLY_CONTEXT(packet) ((packet) = &g_isp_reply)
#else
#define DEVICE_CONTEXT(state) ((void)(state))
#define REQUEST_CONTEXT(packet) ((void)(packet))
#define REPLY_CONTEXT(packet) ((void)(packet))
#endif

#if defined(ISP_DEVICE_BOOT)
#define ISP_MODE(device) ISP_MODE_BOOT
#elif defined(ISP_DEVICE_APP)
#define ISP_MODE(device) ISP_MODE_APP
#elif defined(ISP_DEVICE_TRANSFER)
#define ISP_MODE(device) ISP_MODE_TRANSFER
#else
#define ISP_MODE(device) ((device)->mode)
#endif

#if defined(ISP_XMODEM)
#define ISP_WRITE_BLOCK_SIZE 128U
#else
#define ISP_WRITE_BLOCK_SIZE 64U
#endif

#if defined(ISP_DEVICE_BOOT) && defined(ISP_XMODEM)
/* Boot has no STATS/CLEAR_STATS command. Keep diagnostic accounting in the
 * App/Transfer images; replay and error acceptance remain identical here.
 */
#define increment(value) ((void)0)
#else
static void increment(unsigned long ISP_RAM *value)
{
    if (*value != 0xFFFFFFFFUL) ++*value;
}
#endif
void IspDeviceInit(IspDevice ISP_RAM *device, const IspDeviceConfig *config,
                   const IspDeviceOps *ops, unsigned char mode)
{
    DEVICE_CONTEXT(device);
    memset(device, 0, sizeof(*device));
    device->config = config; device->ops = ops; device->mode = mode;
    device->mode = ISP_MODE(device);
}
void IspDeviceInstalled(IspDevice ISP_RAM *device, unsigned long version,
                        unsigned long build, unsigned long size,
                        unsigned char version_control)
{
    DEVICE_CONTEXT(device);
    device->valid = 1U; device->installed_version = version;
    device->installed_build = build; device->installed_size = size;
    device->version_control = version_control;
}
#if !defined(ISP_DEVICE_BOOT)
void IspDeviceSetDiagnostics(IspDevice ISP_RAM *device,
    void (*read)(unsigned char *bytes))
{
    device->diagnostics = read;
}
#endif
static unsigned char same(const IspPacket ISP_RAM *a, const IspPacket ISP_RAM *b)
{
    /* Sequence/type/status were already checked by Process. */
    return (unsigned char)(a->command == b->command && a->length == b->length &&
        IspBytesEqual(a->data, b->data, (unsigned char)a->length));
}
static void info(IspDevice ISP_RAM *device, IspPacket ISP_RAM *reply)
{
    const IspDeviceConfig *c;
    DEVICE_CONTEXT(device);
    REPLY_CONTEXT(reply);
    c = device->config;
    reply->length = 88U;
    /* The checked layout is the little-endian wire INFO template. */
    memcpy(reply->data, c, 88U);
    reply->data[1] = device->mode;
    reply->data[6] = device->valid; reply->data[7] = device->active;
    IspBytesCopy(reply->data + 40U, (const unsigned char ISP_RAM *)&device->installed_version, 12U);
    IspBytesCopy(reply->data + 52U, &device->version_control, 2U);
    reply->data[54] = (ISP_MODE(device) == ISP_MODE_BOOT) ? 0x0FU :
        ((ISP_MODE(device) == ISP_MODE_APP) ? 0x3CU : 0x10U);
#if !defined(ISP_DEVICE_BOOT)
    if (device->diagnostics && ISP_MODE(device) != ISP_MODE_BOOT)
        reply->data[54] |= ISP_CAP_DIAGNOSTICS;
#endif
#if defined(ISP_DEVICE_TRANSFER) || (!defined(ISP_DEVICE_BOOT) && !defined(ISP_DEVICE_APP))
#if !defined(ISP_DEVICE_TRANSFER)
    if (ISP_MODE(device) == ISP_MODE_TRANSFER)
#endif
    {
        /* A standalone transfer test does not report installed Boot/App. */
        memset(reply->data + 8U, 0, 28U);
        memset(reply->data + 40U, 0, 14U);
        memset(reply->data + 84U, 0, 4U);
    }
#endif
}
#if !defined(ISP_DEVICE_APP) && !defined(ISP_DEVICE_TRANSFER)
static unsigned char stage(IspDevice ISP_RAM *device, const IspPacket ISP_RAM *request)
{
    unsigned char at, size;
    unsigned char ISP_RAM *bytes;
    DEVICE_CONTEXT(device);
    REQUEST_CONTEXT(request);
    if (device->active) return ISP_BAD_STATE;
    if (request->length < 3U || request->length > 66U) return ISP_BAD_SIZE;
    at = request->data[0]; size = (unsigned char)(request->length - 2U);
    if (request->data[1] || (at & 3U) || (size & 3U) || at > 252U || size > 256U - at)
        return ISP_BAD_SIZE;
    bytes = (unsigned char ISP_RAM *)device->metadata;
    if (at == 0U) device->staged = 0U;
    if (at != device->staged) return ISP_BAD_SEQUENCE;
    IspBytesCopy(bytes + at, request->data + 2U, size);
    device->staged = (unsigned short)(device->staged + size);
    return ISP_OK;
}
static unsigned char start(IspDevice ISP_RAM *device, const IspPacket ISP_RAM *request)
{
    unsigned long size;
    DEVICE_CONTEXT(device);
    REQUEST_CONTEXT(request);
    if (device->active || device->staged != 256U) return ISP_BAD_STATE;
    if (request->length != 8U) return ISP_BAD_SIZE;
    size = IspRead32(request->data);
    if (size < 516UL || size > ISP_CFG(capacity) || (size & 3UL) ||
        ISP_CFG(erase_size) < 64UL || (ISP_CFG(erase_size) & (ISP_CFG(erase_size) - 1UL)) ||
        (ISP_CFG(capacity) & (ISP_CFG(erase_size) - 1UL)) ||
        ISP_CFG(app_start) < ISP_CFG(boot_size) || (ISP_CFG(app_start) & (ISP_CFG(erase_size) - 1UL)) ||
        ISP_CFG(app_start) >= ISP_CFG(flash_size) || ISP_CFG(capacity) > ISP_CFG(flash_size) - ISP_CFG(app_start) ||
        !ISP_CALLBACKS_VALID)
        return ISP_BAD_METADATA;
    if (!ISP_METADATA(device->metadata) || device->metadata[6] != size)
        return ISP_BAD_METADATA;
    if (device->valid && device->version_control &&
        request->command != ISP_CMD_FORCE_START &&
        device->metadata[4] < device->installed_version) return ISP_BAD_VERSION;
    device->image_size = size; device->image_crc = IspRead32(request->data + 4U);
    device->next_offset = 0UL; device->active = 1U; device->dirty = 0U;
    return ISP_OK;
}
unsigned char IspDeviceWrite(IspDevice ISP_RAM *device, unsigned long offset,
    const unsigned char ISP_RAM *data, unsigned short count)
{
    unsigned long address;
    unsigned short i;
    unsigned long remaining;
    DEVICE_CONTEXT(device);
    if (!device->active) return ISP_BAD_STATE;
    if (count < 4U || count > ISP_WRITE_BLOCK_SIZE) return ISP_BAD_SIZE;
    if (offset != device->next_offset) return ISP_BAD_SEQUENCE;
    if (offset >= device->image_size || (offset & (ISP_WRITE_BLOCK_SIZE - 1UL))) return ISP_BAD_SIZE;
    remaining = device->image_size - offset;
    if (count != ((remaining > ISP_WRITE_BLOCK_SIZE) ? ISP_WRITE_BLOCK_SIZE : (unsigned short)remaining)) return ISP_BAD_SIZE;
    if (offset >= 256UL && offset < 512UL &&
        !IspBytesEqual(data, (const unsigned char ISP_RAM *)device->metadata +
               (unsigned short)(offset - 256UL), (unsigned char)count)) return ISP_BAD_METADATA;
    address = ISP_CFG(app_start) + offset;
    /* Validate the whole block before any Flash callback. An aligned 64/128
     * byte block never crosses a supported 1 KiB erase boundary.
     */
    if (address < ISP_CFG(app_start) || offset > ISP_CFG(capacity) ||
        count > ISP_CFG(capacity) - offset) return ISP_BAD_SIZE;
    device->valid = 0U; device->ready = 0U; device->dirty = 1U;
    if ((address & (ISP_CFG(erase_size) - 1UL)) == 0UL && !ISP_ERASE(address)) {
        device->active = 0U; return ISP_FLASH_ERROR;
    }
    for (i = 0U; i < count; i = (unsigned short)(i + 4U)) {
        if (!ISP_WRITE(address + i, data + i)) {
            device->active = 0U; return ISP_FLASH_ERROR;
        }
#if !defined(ISP_DEVICE_BOOT) || !defined(ISP_XMODEM)
        device->written = (device->written > 0xFFFFFFFBUL) ? 0xFFFFFFFFUL : device->written + 4UL;
#endif
    }
    device->next_offset += count;
    return ISP_OK;
}
unsigned char IspDeviceFinish(IspDevice ISP_RAM *device)
{
    DEVICE_CONTEXT(device);
    if (!device->active || device->next_offset != device->image_size) return ISP_BAD_STATE;
    /* The adapter verifies the stored trailer against this expected CRC,
     * actual image CRC, metadata size/contract and complete App IRQ table.
     * Do not perform a second partial validation ahead of that full gate.
     */
    if (!ISP_VERIFY(device->image_size, device->image_crc))
        return ISP_CRC_ERROR;
    device->active = 0U; device->action = ISP_ACTION_RESET; return ISP_OK;
}
#endif
static unsigned char dispatch(IspDevice ISP_RAM *device, const IspPacket ISP_RAM *request,
                               IspPacket ISP_RAM *reply)
{
#if !defined(ISP_DEVICE_TRANSFER)
    unsigned short at, size;
#if !defined(ISP_DEVICE_APP)
    unsigned long crc;
    unsigned char bytes[4];
#endif
#endif
    DEVICE_CONTEXT(device);
    REQUEST_CONTEXT(request); REPLY_CONTEXT(reply);
    if (request->command == ISP_CMD_INFO && request->length == 0U) {
        info(device, reply); return ISP_OK;
    }
#if !defined(ISP_DEVICE_BOOT)
#if defined(ISP_XMODEM) || !defined(__CCRL__)
    if (device->config->contract == 3U && request->command == ISP_CMD_STREAM_TEST &&
        request->length == 8U && !device->active) {
        device->image_size = IspRead32(request->data);
        if (!device->image_size || device->image_size > 65536UL) return ISP_BAD_SIZE;
        device->image_crc = IspRead32(request->data + 4U);
        device->active = 1U; device->action = ISP_ACTION_STREAM_TEST; return ISP_OK;
    }
#endif
    if (request->command == ISP_CMD_PING || request->command == ISP_CMD_ECHO) {
        reply->length = request->length;
        IspBytesCopy(reply->data, request->data, (unsigned char)request->length); return ISP_OK;
    }
#if !defined(ISP_DEVICE_BOOT)
    if (request->command == ISP_CMD_DIAGNOSTICS && request->length == 0U &&
        device->diagnostics && ISP_MODE(device) != ISP_MODE_BOOT) {
        reply->length = ISP_DIAGNOSTIC_BYTES;
        device->diagnostics(reply->data); return ISP_OK;
    }
#endif
    if (request->command == ISP_CMD_STATS && request->length == 0U) {
        reply->length = 16U;
        IspWrite32(reply->data, device->requests);
        IspWrite32(reply->data + 4U, device->replays);
        IspWrite32(reply->data + 8U, device->errors);
        IspWrite32(reply->data + 12U, device->written); return ISP_OK;
    }
    if (request->command == ISP_CMD_CLEAR_STATS && request->length == 0U) {
        device->requests = 0UL; device->replays = 0UL;
        device->errors = 0UL; device->written = 0UL; return ISP_OK;
    }
#endif
#if defined(ISP_DEVICE_TRANSFER)
    return ISP_BAD_COMMAND;
#else
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_DEVICE_APP)
    if (ISP_MODE(device) == ISP_MODE_TRANSFER) return ISP_BAD_COMMAND;
#endif
    if (request->command == ISP_CMD_ENTER && request->length == 0U && !device->active) {
#if defined(ISP_DEVICE_APP)
        device->action = ISP_ACTION_ENTER;
#elif !defined(ISP_DEVICE_BOOT)
        if (ISP_MODE(device) == ISP_MODE_APP) device->action = ISP_ACTION_ENTER;
#endif
        return ISP_OK;
    }
    if (request->command == ISP_CMD_READ_METADATA && request->length == 3U &&
        device->valid && ISP_CAN_READ) {
        at = (unsigned short)request->data[0] | ((unsigned short)request->data[1] << 8);
        size = request->data[2];
        if (at > 255U || !size || size > 64U || size > 256U - at) return ISP_BAD_SIZE;
        if (!ISP_READ(ISP_CFG(app_start) + 256UL + at, reply->data, size))
            return ISP_BAD_STATE;
        reply->length = size; return ISP_OK;
    }
#if defined(ISP_DEVICE_APP)
    return ISP_BAD_STATE;
#else
#if !defined(ISP_DEVICE_BOOT)
    if (ISP_MODE(device) != ISP_MODE_BOOT) return ISP_BAD_STATE;
#endif
    if (request->command == ISP_CMD_METADATA) return stage(device, request);
    if (request->command == ISP_CMD_START || request->command == ISP_CMD_FORCE_START)
        return start(device, request);
#if !defined(ISP_XMODEM)
    if (request->command == ISP_CMD_WRITE) {
        if (!device->active) return ISP_BAD_STATE;
        if (request->length < 8U || request->length > 68U) return ISP_BAD_SIZE;
        return IspDeviceWrite(device, IspRead32(request->data), request->data + 4U,
            (unsigned short)(request->length - 4U));
    }
#endif
    if (request->command == ISP_CMD_ABORT && request->length == 0U) {
        device->active = 0U; device->staged = 0U; return ISP_OK;
    }
#if !defined(ISP_XMODEM)
    if (request->command == ISP_CMD_END && request->length == 0U)
        return IspDeviceFinish(device);
#endif
    if (request->command == ISP_CMD_VERIFY && request->length == 0U && device->valid &&
        !device->active && ISP_CAN_READ) {
        if (!ISP_READ(ISP_CFG(app_start) + device->installed_size - 4UL, bytes, 4U))
            return ISP_CRC_ERROR;
        crc = IspRead32(bytes);
        if (!ISP_VERIFY(device->installed_size, crc)) return ISP_CRC_ERROR;
        reply->length = 8U; IspWrite32(reply->data, device->installed_size);
        IspWrite32(reply->data + 4U, crc); return ISP_OK;
    }
    return ISP_BAD_COMMAND;
#endif
#endif
}
void IspDeviceProcess(IspDevice ISP_RAM *device, const IspPacket ISP_RAM *request, IspPacket ISP_RAM *reply)
{
    unsigned char status;
    DEVICE_CONTEXT(device);
    REQUEST_CONTEXT(request); REPLY_CONTEXT(reply);
    reply->length = 0U;
    reply->response = 1U; reply->sequence = request->sequence;
    reply->command = request->command;
    if (!request->sequence || request->length > ISP_PAYLOAD_MAX || request->response || request->status) {
        reply->status = ISP_BAD_SIZE; return;
    }
    increment(&device->requests);
    device->action = ISP_ACTION_NONE;
    if (device->cached && request->sequence == device->last_request.sequence) {
        if (same(request, &device->last_request)) {
            IspBytesCopy((unsigned char ISP_RAM *)reply,
                (const unsigned char ISP_RAM *)&device->last_reply, (unsigned char)sizeof(*reply));
            increment(&device->replays); return;
        }
        reply->status = ISP_BAD_SEQUENCE; increment(&device->errors); return;
    }
    status = dispatch(device, request, reply);
    reply->status = status;
    if (status != ISP_OK) { reply->length = 0U; increment(&device->errors); }
    IspBytesCopy((unsigned char ISP_RAM *)&device->last_request,
        (const unsigned char ISP_RAM *)request, (unsigned char)sizeof(*request));
    IspBytesCopy((unsigned char ISP_RAM *)&device->last_reply,
        (const unsigned char ISP_RAM *)reply, (unsigned char)sizeof(*reply));
    device->cached = 1U;
}
