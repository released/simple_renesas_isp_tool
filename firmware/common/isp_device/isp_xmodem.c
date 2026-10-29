/* Project/API reading notes: XMODEM-CRC 128-byte block receiver.
 * Block CRC16 protects the serial transfer; final metadata/vector/full-image CRC32 checks remain
 * independent.
 * Handle duplicate blocks, cancellation and failed control-byte TX without executing a pending
 * reset.
 */
#include "isp_xmodem.h"
#include <string.h>
#if defined(__CCRL__) || defined(ISP_TEST_XMODEM_CONTEXT)
IspXmodemInput g_isp_input;
#define X_CONTEXT(state) ((state) = &g_isp_input.x)
#else
#define X_CONTEXT(x) ((void)(x))
#endif
#if defined(__CCRL__) || defined(ISP_TEST_XMODEM_DIRECT)
#define X_WRITE(at, data, size) IspXmodemWrite(at, data, size)
#define X_FINISH() IspXmodemFinish()
#define X_ABORT() IspXmodemAbort()
#define X_SEND(byte) IspXmodemSend(byte)
#else
#define X_WRITE(at, data, size) ops->write(at, data, size)
#define X_FINISH() ops->finish()
#define X_ABORT() ops->abort()
#define X_SEND(byte) ops->send(byte)
#endif
unsigned short IspXmodemCrc(const unsigned char ISP_RAM *data, unsigned short size)
{
    unsigned short crc, at;
    unsigned char bit;
    crc = 0U;
    for (at = 0U; at < size; ++at)
    {
        crc ^= (unsigned short)data[at] << 8;
        for (bit = 0U; bit < 8U; ++bit)
        {
            crc = (unsigned short)((crc & 0x8000U) ? (crc << 1) ^ 0x1021U : crc << 1);
        }
    }
    return crc;
}
void IspXmodemInit(IspXmodem ISP_RAM *x, unsigned long size, unsigned short now)
{
    X_CONTEXT(x);
    memset(x, 0, sizeof(*x));
    x->size = size;
    x->block = 1U;
    x->active = 1U;
    x->last_tick = now;
    x->last_prompt = now;
}
static void cancel(IspXmodem ISP_RAM *x, const IspXmodemOps *ops)
{
    X_CONTEXT(x);
    (void)ops;
    x->active = 0U;
    x->canceled = 1U;
    X_ABORT();
    X_SEND(0x18U);
    X_SEND(0x18U);
}
static unsigned char block(IspXmodem ISP_RAM *x, const IspXmodemOps *ops)
{
    unsigned short crc;
    unsigned char size, at;
    X_CONTEXT(x);
    (void)ops;
    crc = (unsigned short)((unsigned short)x->data[130] << 8) | x->data[131];
    if ((unsigned char)(x->data[0] ^ x->data[1]) != 0xFFU ||
        IspXmodemCrc(x->data + 2U, 128U) != crc)
    {
        return 0x15U;
    }
    if (x->have_previous && x->data[0] == (unsigned char)(x->block - 1U))
    {
        return (unsigned char)(IspBytesEqual(x->data + 2U, x->previous, 128U) ? 6U : 0x18U);
    }
    if (x->data[0] != x->block || x->offset >= x->size)
    {
        return 0x18U;
    }
    size = (x->size - x->offset >= 128UL) ? 128U : (unsigned char)(x->size - x->offset);
    for (at = size; at < 128U; ++at)
    {
        if (x->data[at + 2U] != 0x1AU)
        {
            return 0x18U;
        }
    }
    if (!X_WRITE(x->offset, x->data + 2U, size))
    {
        return 0x18U;
    }
    IspBytesCopy(x->previous, x->data + 2U, 128U);
    x->offset += size;
    ++x->block;
    x->have_previous = 1U;
    return 6U;
}
void IspXmodemFeed(IspXmodem ISP_RAM *x, unsigned char byte, unsigned short now,
                   const IspXmodemOps *ops)
{
    X_CONTEXT(x);
    (void)ops;
    if (!x->active)
    {
        return;
    }
    x->last_tick = now;
    if (!x->count)
    {
        if (byte == 0x18U)
        {
            if (++x->can_count == 2U)
            {
                cancel(x, ops);
            }
            return;
        }
        x->can_count = 0U;
        if (byte == 4U)
        {
            /* No Flash operation is repeated by a second EOT. */
            if (x->offset != x->size || !X_FINISH())
            {
                cancel(x, ops);
                return;
            }
            x->active = 0U;
            X_SEND(6U);
            return;
        }
        if (byte == 1U)
        {
            x->count = 1U;
        }
        else
        {
            X_SEND(0x15U);
        }
        return;
    }
    x->data[x->count - 1U] = byte;
    if (++x->count != 133U)
    {
        return;
    }
    x->count = 0U;
    byte = block(x, ops);
    if (byte == 0x18U)
    {
        cancel(x, ops);
    }
    else
    {
        X_SEND(byte);
    }
}
void IspXmodemPoll(IspXmodem ISP_RAM *x, unsigned short now, const IspXmodemOps *ops)
{
    X_CONTEXT(x);
    if (!x->active)
    {
        return;
    }
    if ((unsigned short)(now - x->last_tick) >= 10000U)
    {
        cancel(x, ops);
        return;
    }
    if (x->count && (unsigned short)(now - x->last_tick) >= 200U)
    {
        x->count = 0U;
        x->can_count = 0U;
        X_SEND(0x15U);
    }
    if (!x->have_previous && !x->count && (unsigned short)(now - x->last_prompt) >= 1000U)
    {
        x->last_prompt = now;
        X_SEND('C');
    }
}
