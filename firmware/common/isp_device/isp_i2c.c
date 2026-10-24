#if defined(ISP_MCU_F13)
#include "r_cg_macrodriver.h"
#else
#include "platform.h"
#endif
#include "isp_transport_config.h"
#include "isp_i2c_mailbox.h"
#include "isp_i2c.h"
#include "isp_rl78_board.h"
static IspI2cMailbox g_mailbox;
static volatile unsigned char g_addressed, g_accepted;
static volatile unsigned short g_last_activity;

/* ISR follows the IICA slave address/data/ACK/STOP sequence. It moves at most
 * one byte per interrupt; Flash and ISP framing remain in the main loop.
 */
#pragma interrupt IspI2cIsr
void IspI2cIsr(void)
{
    unsigned char byte;
    g_last_activity = IspTick();
    if (SPD0) {
        IspMailboxStop(&g_mailbox); g_addressed = 0U; g_accepted = 0U;
        WREL0 = 1U; return;
    }
    if (!g_addressed) {
        if (!COI0) { LREL0 = 1U; return; }
        g_addressed = 1U;
        g_accepted = IspMailboxStart(&g_mailbox, (unsigned char)TRC0);
        if (!g_accepted) { ACKE0 = 0U; WREL0 = 1U; return; }
        ACKE0 = 1U;
        if (TRC0) { WTIM0 = 1U; IICA0 = IspMailboxNext(&g_mailbox); }
        else { WTIM0 = 0U; WREL0 = 1U; }
        return;
    }
    if (!g_accepted) { ACKE0 = 0U; WREL0 = 1U; return; }
    if (TRC0) {
        if (!ACKD0) { IspMailboxNack(&g_mailbox); WREL0 = 1U; }
        else IICA0 = IspMailboxNext(&g_mailbox);
    } else {
        byte = IICA0;
        if (!IspMailboxReceive(&g_mailbox, byte)) ACKE0 = 0U;
        WREL0 = 1U;
    }
}
void IspI2cInit(void)
{
    unsigned char mask;
    mask = IICAMK0; IICAMK0 = 1U;
    IspMailboxInit(&g_mailbox); g_addressed = 0U; g_accepted = 0U;
    IICA0EN = 1U; IICE0 = 0U; IICAIF0 = 0U;
    IICAPR10 = 1U; IICAPR00 = 1U;
#if defined(ISP_MCU_F13) || defined(ISP_MCU_F24)
    P6 &= 0xF3U; PM6 |= 0x0CU;
#if defined(ISP_MCU_F24)
    POM6 |= 0x0CU;
#endif
#else
    /* These two pins have dedicated open-drain IICA outputs on G16/G22. */
    P6 &= 0xFCU; PM6 |= 3U;
#endif
    SMC0 = 0U; DFC0 = 0U; IICRSV0 = 1U;
#if defined(ISP_SCFG_IICA_PRS)
#if !defined(ISP_MCU_G16)
    IICCTL01 = (unsigned char)((IICCTL01 & 0xFEU) | ISP_SCFG_IICA_PRS);
#endif
    IICWL0 = ISP_SCFG_IICA_WL; IICWH0 = ISP_SCFG_IICA_WH;
#elif defined(ISP_MCU_F24)
    IICCTL01 |= 1U;
    IICWL0 = 96U; IICWH0 = 105U;
#else
#if !defined(ISP_MCU_G16)
    IICCTL01 |= 1U;
#endif
    IICWL0 = 76U; IICWH0 = 85U;
#endif
    SVA0 = (unsigned char)(ISP_I2C_ADDRESS << 1);
    STCEN0 = 1U; SPIE0 = 1U; WTIM0 = 1U; ACKE0 = 1U;
    IICE0 = 1U; LREL0 = 1U;
#if defined(ISP_MCU_F13) || defined(ISP_MCU_F24)
    PM6 &= 0xF3U;
#else
    PM6 &= 0xFCU;
#endif
    g_last_activity = IspTick();
    IICAMK0 = 0U;
    (void)mask;
}
void IspI2cStop(void) { IICAMK0 = 1U; IICE0 = 0U; IICAIF0 = 0U; }
void IspI2cPoll(unsigned short now)
{
    unsigned char mask;
    if (g_mailbox.busy && (unsigned short)(now - g_last_activity) >= 2000U) {
        mask = IICAMK0; IICAMK0 = 1U;
        if ((unsigned short)(now - g_last_activity) >= 2000U) {
            IspMailboxInit(&g_mailbox); g_addressed = 0U; g_accepted = 0U;
        }
        IICAMK0 = mask;
    }
}
unsigned char IspI2cGet(unsigned char *value) { return IspMailboxGet(&g_mailbox, value); }
unsigned char IspI2cSend(const unsigned char *data, unsigned short size)
{
    if (size > ISP_FRAME_MAX) return 0U;
    return IspMailboxPublish(&g_mailbox, data, (unsigned char)size);
}
void IspI2cRelease(void) { IspMailboxRelease(&g_mailbox); }
unsigned char IspI2cReplyComplete(void) { return g_mailbox.reply_done; }
