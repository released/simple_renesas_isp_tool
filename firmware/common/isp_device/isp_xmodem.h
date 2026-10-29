/* Project/API reading notes: XMODEM-CRC streaming state and callbacks.
 * The application supplies a bounded sink; Boot writes through its guarded updater while
 * App/Transfer diagnostic sinks do not modify Flash.
 */
#ifndef ISP_XMODEM_H
#define ISP_XMODEM_H
#include "isp_protocol.h"
#ifdef __cplusplus
extern "C"
{
#endif
    /* XMODEM-CRC128 receiver. The caller owns UART deadlines and all Flash gates.
     * Duplicate blocks are compared byte-for-byte and never committed twice.
     * EOT finishes once only; a lost final ACK is an unknown result, not a retry.
     */
    typedef struct
    {
        unsigned char count;
        unsigned short last_tick, last_prompt;
        unsigned long size, offset;
        unsigned char block, have_previous, active, canceled, can_count;
        /* Scalar state stays in the short indexed-address window. */
        unsigned char data[132], previous[128];
    } IspXmodem;
    typedef union
    {
        IspStream stream;
        IspXmodem x;
    } IspXmodemInput;
#if defined(__CCRL__) || defined(ISP_TEST_XMODEM_CONTEXT)
    /* Each device image owns one UART update session. Share its input RAM with
     * the inactive framed control parser; this never merges physical buses.
     */
    extern IspXmodemInput g_isp_input;
#endif
    typedef struct
    {
        unsigned char (*write)(unsigned long offset, const unsigned char ISP_RAM *data,
                               unsigned short size);
        unsigned char (*finish)(void);
        void (*abort)(void);
        void (*send)(unsigned char byte);
    } IspXmodemOps;
/* MCU images have one statically selected adapter. The native model also
 * exercises this binding; no indirect ROM callback is needed on the MCU.
 */
#if defined(__CCRL__) || defined(ISP_TEST_XMODEM_DIRECT)
    unsigned char IspXmodemWrite(unsigned long offset, const unsigned char ISP_RAM *data,
                                 unsigned short size);
    unsigned char IspXmodemFinish(void);
    void IspXmodemAbort(void);
    void IspXmodemSend(unsigned char byte);
#endif
    /* Receiver and block bytes are data RAM; only the callback table is ROM. */
    unsigned short IspXmodemCrc(const unsigned char ISP_RAM *data, unsigned short size);
    void IspXmodemInit(IspXmodem ISP_RAM *x, unsigned long size, unsigned short now);
    void IspXmodemFeed(IspXmodem ISP_RAM *x, unsigned char byte, unsigned short now,
                       const IspXmodemOps *ops);
    void IspXmodemPoll(IspXmodem ISP_RAM *x, unsigned short now, const IspXmodemOps *ops);
#ifdef __cplusplus
}
#endif
#endif
