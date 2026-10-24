#include "isp_protocol.h"
#include <string.h>

unsigned char IspDecode(const unsigned char ISP_RAM *bytes, unsigned short size, IspPacket ISP_RAM *packet)
{
    unsigned short length;
    if (size < 14U || bytes[0] != 0x49U || bytes[1] != 0x53U || bytes[2] != 1U || bytes[3] > 1U) return 0U;
    length = (unsigned short)bytes[8] | ((unsigned short)bytes[9] << 8);
    if (length > ISP_PAYLOAD_MAX || size != length + 14U ||
        IspCrc32(bytes, (unsigned short)(size - 4U)) != IspRead32(bytes + size - 4U)) return 0U;
    packet->response = bytes[3]; packet->sequence = (unsigned short)bytes[4] | ((unsigned short)bytes[5] << 8);
    packet->command = bytes[6]; packet->status = bytes[7]; packet->length = length;
    memcpy(packet->data, bytes + 10U, length); return 1U;
}
unsigned long IspRead32(const unsigned char ISP_RAM *p)
{
    /* Byte loads accept unaligned wire buffers without a word dereference. */
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
        ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
void IspWrite32(unsigned char ISP_RAM *p, unsigned long value)
{
    p[0] = (unsigned char)value; p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16); p[3] = (unsigned char)(value >> 24);
}
unsigned char IspBytesEqual(const unsigned char ISP_RAM *a,
    const unsigned char ISP_RAM *b, unsigned char size)
{
    while (size) {
        if (*a++ != *b++) return 0U;
        --size;
    }
    return 1U;
}
void IspBytesCopy(unsigned char ISP_RAM *to,
    const unsigned char ISP_RAM *from, unsigned char size)
{
    while (size) { *to++ = *from++; --size; }
}
unsigned long IspCrc32(const unsigned char ISP_RAM *data, unsigned short size)
{
#if defined(__CCRL__)
    const unsigned char __far *far_data;
    unsigned long caculate_crc32_checksum(unsigned long address, unsigned long length);
    far_data = data;
    return caculate_crc32_checksum((unsigned long)far_data, (unsigned long)size);
#else
    unsigned long crc;
    unsigned short i;
    unsigned char bit;
    crc = 0xFFFFFFFFUL;
    for (i = 0U; i < size; ++i) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1) ^ ((crc & 1UL) ? 0xEDB88320UL : 0UL);
    }
    return crc ^ 0xFFFFFFFFUL;
#endif
}
unsigned short IspEncode(const IspPacket ISP_RAM *packet, unsigned char ISP_RAM *output)
{
    unsigned short length;
    if (packet == 0 || output == 0 || packet->length > ISP_PAYLOAD_MAX ||
        packet->sequence == 0U || packet->response > 1U ||
        (!packet->response && packet->status != 0U)) return 0U;
    length = (unsigned short)(ISP_HEADER_SIZE + packet->length);
    output[0] = 0x49U; output[1] = 0x53U;
    output[2] = 1U; output[3] = packet->response;
    output[4] = (unsigned char)packet->sequence;
    output[5] = (unsigned char)(packet->sequence >> 8);
    output[6] = packet->command; output[7] = packet->status;
    output[8] = (unsigned char)packet->length; output[9] = 0U;
    IspBytesCopy(output + ISP_HEADER_SIZE, packet->data, (unsigned char)packet->length);
    IspWrite32(output + length, IspCrc32(output, length));
    return (unsigned short)(length + 4U);
}
void IspStreamInit(IspStream ISP_RAM *stream)
{
#if defined(ISP_XMODEM) && defined(ISP_DEVICE_BOOT)
    /* Control bytes are written before use; Boot exposes no parser counters.
     * The same RAM may contain the just-finished XMODEM receiver state.
     */
    stream->count = 0U;
#else
    memset(stream, 0, sizeof(*stream));
#endif
}
static void discard(IspStream ISP_RAM *stream)
{
    --stream->count;
    memmove(stream->bytes, stream->bytes + 1U, stream->count);
}
unsigned char IspStreamFeed(IspStream ISP_RAM *stream, unsigned char value,
                            IspPacket ISP_RAM *packet)
{
    unsigned short length, total;
    unsigned char ISP_RAM *p;
    if (stream->count >= ISP_FRAME_MAX) discard(stream);
    stream->bytes[stream->count++] = value;
    p = stream->bytes;
    for (;;) {
        if (!stream->count) return 0U;
        if (p[0] != 0x49U) { discard(stream); continue; }
        if (stream->count < 2U) return 0U;
        if (p[1] != 0x53U) { discard(stream); continue; }
        if (stream->count < ISP_HEADER_SIZE) return 0U;
        length = (unsigned short)p[8] | ((unsigned short)p[9] << 8);
        if (p[2] != 1U || p[3] > 1U || (p[4] == 0U && p[5] == 0U) ||
            (!p[3] && p[7]) || length > ISP_PAYLOAD_MAX) {
#if !defined(ISP_XMODEM) || !defined(ISP_DEVICE_BOOT)
            if (stream->framing_errors != 0xFFFFFFFFUL) ++stream->framing_errors;
#endif
            discard(stream); continue;
        }
        total = (unsigned short)(ISP_HEADER_SIZE + length + 4U);
        if (stream->count < total) return 0U;
        if (IspRead32(p + total - 4U) != IspCrc32(p, (unsigned short)(total - 4U))) {
#if !defined(ISP_XMODEM) || !defined(ISP_DEVICE_BOOT)
            if (stream->crc_errors != 0xFFFFFFFFUL) ++stream->crc_errors;
#endif
            discard(stream); continue;
        }
        packet->sequence = (unsigned short)p[4] | ((unsigned short)p[5] << 8);
        packet->command = p[6]; packet->status = p[7]; packet->response = p[3];
        packet->length = length;
        IspBytesCopy(packet->data, p + ISP_HEADER_SIZE, (unsigned char)length);
#if defined(ISP_XMODEM)
        stream->count = (unsigned char)(stream->count - total);
#else
        stream->count = (unsigned short)(stream->count - total);
#endif
        memmove(p, p + total, stream->count);
        return 1U;
    }
}
