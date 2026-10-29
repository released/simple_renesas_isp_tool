/* Project/API reading notes: Bounded I2C request/reply mailbox.
 * Preserve request boundaries and unread reply state across byte events; reset/handoff depends on
 * a proven complete reply, not merely loading the transmit register.
 */
#include "isp_i2c_mailbox.h"
void IspMailboxInit(IspI2cMailbox *box)
{
    unsigned char *p;
    unsigned short at;
    p = (unsigned char *)box;
    for (at = 0U; at < sizeof(*box); ++at)
    {
        p[at] = 0U;
    }
}
unsigned char IspMailboxStart(IspI2cMailbox *box, unsigned char reading)
{
    box->sending = reading;
    box->writing = 0U;
    if (reading)
    {
        box->snapshot_ready = box->reply_ready;
        box->send_count = 0U;
        box->final_nack = 0U;
        return 1U;
    }
    if (box->busy)
    {
        return 0U;
    }
    box->writing = 1U;
    box->busy = 1U;
    box->receive_count = 0U;
    box->receive_at = 0U;
    box->ready = 0U;
    box->drained = 0U;
    box->overflow = 0U;
    return 1U;
}
unsigned char IspMailboxReceive(IspI2cMailbox *box, unsigned char value)
{
    if (!box->writing || box->sending || box->ready || !box->busy ||
        box->receive_count >= ISP_FRAME_MAX)
    {
        box->overflow = 1U;
        return 0U;
    }
    box->receive[box->receive_count++] = value;
    return 1U;
}
unsigned char IspMailboxNext(IspI2cMailbox *box)
{
    unsigned char at;
    at = box->send_count;
    if (at >= ISP_I2C_REPLY_BYTES)
    {
        return 0xFFU;
    }
    box->send_count = (unsigned char)(at + 1U);
    if (!at)
    {
        return box->snapshot_ready ? ISP_I2C_READY : ISP_I2C_BUSY;
    }
    if (box->snapshot_ready && at <= box->reply_length)
    {
        return box->reply[at - 1U];
    }
    return 0xFFU;
}
void IspMailboxNack(IspI2cMailbox *box)
{
    box->final_nack = 1U;
}
void IspMailboxStop(IspI2cMailbox *box)
{
    if (box->sending)
    {
        if (box->snapshot_ready && box->send_count == ISP_I2C_REPLY_BYTES && box->final_nack)
        {
            box->reply_ready = 0U;
            box->busy = 0U;
            box->reply_done = 1U;
        }
    }
    else if (box->writing && box->busy && !box->ready && !box->reply_ready)
    {
        if (!box->overflow && box->receive_count >= 14U)
        {
            box->ready = 1U;
        }
        else
        {
            box->busy = 0U;
        }
    }
    box->writing = 0U;
}
unsigned char IspMailboxGet(IspI2cMailbox *box, unsigned char *value)
{
    if (!box->ready || box->receive_at >= box->receive_count)
    {
        return 0U;
    }
    *value = box->receive[box->receive_at++];
    if (box->receive_at == box->receive_count)
    {
        box->ready = 0U;
        box->drained = 1U;
    }
    return 1U;
}
void IspMailboxRelease(IspI2cMailbox *box)
{
    /* Main calls this only after processing the last request byte. An invalid
     * frame has no reply and must not leave the mailbox permanently busy. */
    if (box->drained && !box->reply_ready)
    {
        box->drained = 0U;
        box->busy = 0U;
    }
}
unsigned char IspMailboxPublish(IspI2cMailbox *box, const unsigned char *reply, unsigned char size)
{
    if (!box->busy || !box->drained || size < 14U || size > ISP_FRAME_MAX || box->reply_ready)
    {
        return 0U;
    }
    box->reply = reply;
    box->reply_length = size;
    box->reply_done = 0U;
    box->drained = 0U;
    box->reply_ready = 1U;
    return 1U;
}
