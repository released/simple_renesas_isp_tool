#ifndef ISP_I2C_MAILBOX_H
#define ISP_I2C_MAILBOX_H
#include "isp_protocol.h"
#define ISP_I2C_REPLY_BYTES (ISP_FRAME_MAX + 1U)
#define ISP_I2C_BUSY (0x00U)
#define ISP_I2C_READY (0xA5U)
/* One STOP-delimited request; replies use a fixed111-byte read window.
 * The IRQ adapter calls only the bounded Start/Receive/Next/Stop methods.
 * Main owns Get/Release/Publish. Published reply storage stays immutable until
 * a full ready read ends with master NACK and STOP. No protocol parsing here.
 */
typedef struct {
    volatile unsigned char busy, ready, drained, overflow, writing;
    volatile unsigned char receive_count, receive_at;
    volatile unsigned char reply_ready, reply_done, reply_length;
    volatile unsigned char sending, snapshot_ready, send_count, final_nack;
    unsigned char receive[ISP_FRAME_MAX];
    const unsigned char *reply;
} IspI2cMailbox;
void IspMailboxInit(IspI2cMailbox *box);
unsigned char IspMailboxStart(IspI2cMailbox *box, unsigned char reading);
unsigned char IspMailboxReceive(IspI2cMailbox *box, unsigned char value);
unsigned char IspMailboxNext(IspI2cMailbox *box);
void IspMailboxNack(IspI2cMailbox *box);
void IspMailboxStop(IspI2cMailbox *box);
unsigned char IspMailboxGet(IspI2cMailbox *box, unsigned char *value);
void IspMailboxRelease(IspI2cMailbox *box);
unsigned char IspMailboxPublish(IspI2cMailbox *box, const unsigned char *reply, unsigned char size);
#endif
