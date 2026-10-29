/* Project/API reading notes: RH850 RIIC mailbox backend.
 * STOP and NACK ordering define transaction completion; account for transmit-register prefetch
 * before declaring a reply fully consumed.
 * ISR code moves bytes/state without parsing commands or writing Flash.
 */
#include "isp_rh850.h"
#include "Config_RIIC1.h"
#include "isp_i2c_mailbox.h"

/* RIIC1 is slave-only; its ISR moves mailbox bytes, never waits for STOP,
 * parses requests or runs Flash. Main publishes a fixed111-byte read reply.
 */
static IspI2cMailbox g_box;
static volatile unsigned char g_addressed, g_reading, g_accepted;
static volatile unsigned char g_nack_seen, g_tx_loaded;
static volatile unsigned short g_bus_activity;
static volatile unsigned long g_bus_errors;
void R_Config_RIIC1_Create_UserInit(void)
{
}
static void NackNext(void)
{
    RIIC1.MR3.UINT32 |= _RIIC_ACKBT_BIT_MODIFICATION_ENABLED;
    RIIC1.MR3.UINT32 |= _RIIC_NACK_TRANSMISSION;
}
static void Address(void)
{
    g_reading = (unsigned char)((RIIC1.CR2.UINT32 & _RIIC_TRANSMIT_MODE) != 0UL);
    g_accepted = IspMailboxStart(&g_box, g_reading);
    g_addressed = 1U;
    g_nack_seen = 0U;
    g_tx_loaded = 0U;
    RIIC1.MR3.UINT32 |= _RIIC_ACKBT_BIT_MODIFICATION_ENABLED;
    RIIC1.MR3.UINT32 &= (unsigned long)~_RIIC_NACK_TRANSMISSION;
    if (!g_accepted)
    {
        NackNext();
    }
}
static void ReceiveByte(void)
{
    unsigned char byte;
    if (!(RIIC1.SR2.UINT32 & _RIIC_RECEIVE_COMPLETE))
    {
        return;
    }
    byte = (unsigned char)RIIC1.DRR.UINT32;
    g_bus_activity = IspTick();
    if (!g_addressed)
    {
        Address();
        return;
    }
    if (!g_reading && g_accepted && !IspMailboxReceive(&g_box, byte))
    {
        NackNext();
    }
}
#pragma interrupt IspI2cRxIsr(enable = false, fpu = true, callt = false)
void IspI2cRxIsr(void)
{
    ReceiveByte();
}
#pragma interrupt IspI2cTxIsr(enable = false, fpu = true, callt = false)
void IspI2cTxIsr(void)
{
    g_bus_activity = IspTick();
    if (!g_addressed)
    {
        Address();
    }
    if (!g_reading || g_nack_seen || (RIIC1.SR2.UINT32 & _RIIC_NACK_DETECTED))
    {
        return;
    }
    RIIC1.DRT.UINT32 = IspMailboxNext(&g_box);
    if (g_tx_loaded < ISP_I2C_REPLY_BYTES + 1U)
    {
        ++g_tx_loaded;
    }
}
#pragma interrupt IspI2cEndIsr(enable = false, fpu = true, callt = false)
void IspI2cEndIsr(void)
{
    /* TEI does not mean the master completed the111-byte read. Only the
     * final NACK and STOP can release a ready reply or permit reset.
     */
    RIIC1.SR2.UINT32 &= (unsigned long)~_RIIC_TRANSMIT_END;
}
#pragma interrupt IspI2cEventIsr(enable = false, fpu = true, callt = false)
void IspI2cEventIsr(void)
{
    unsigned long flags;
    unsigned char completed;
    volatile unsigned long synced;
    volatile unsigned char discarded;
    flags = RIIC1.SR2.UINT32;
    g_bus_activity = IspTick();
    if (flags & (_RIIC_ARBITRATION_LOST | _RIIC_TIMEOUT_DETECTED))
    {
        ++g_bus_errors;
        g_box.overflow = 1U;
        g_box.snapshot_ready = 0U;
        RIIC1.SR2.UINT32 &= (unsigned long)~(_RIIC_ARBITRATION_LOST | _RIIC_TIMEOUT_DETECTED);
    }
    if ((flags & _RIIC_NACK_DETECTED) && !g_nack_seen)
    {
        g_nack_seen = 1U;
        /* DRT/DRS are double buffered (hardware manual23.3.15). A loaded
         * byte may still be waiting in DRT. Prove the whole read window
         * left DRT before releasing the reply; a short read keeps it.
         * STOP clears TDRE, so a coalesced STOP is conservatively bounded
         * by loaded-1, not guessed from the number of ISR calls.
         */
        completed = g_tx_loaded;
        if (completed && !(flags & _RIIC_TRANSMIT_DATA_EMPTY))
        {
            --completed;
        }
        discarded = (unsigned char)RIIC1.DRR.UINT32;
        (void)discarded;
        if (g_reading && completed >= ISP_I2C_REPLY_BYTES)
        {
            IspMailboxNack(&g_box);
        }
        /* Keep NACKF latched until STOP (manual23.4 caution5). Mask its
         * level request while waiting; never wait for STOP inside the ISR.
         */
        RIIC1.IER.UINT32 &= (unsigned long)~_RIIC_NACK_RECEPTION_INT_ENABLE;
    }
    if (flags & _RIIC_STOP_CONDITION_DETECTED)
    {
        /* STOP and the last receive IRQ can be pending together. Drain
         * that single hardware byte before publishing the request.
         */
        if (!g_reading && (RIIC1.SR2.UINT32 & _RIIC_RECEIVE_COMPLETE))
        {
            ReceiveByte();
        }
        IspMailboxStop(&g_box);
        g_addressed = 0U;
        g_accepted = 0U;
        RIIC1.SR2.UINT32 &= (unsigned long)~(_RIIC_STOP_CONDITION_DETECTED | _RIIC_NACK_DETECTED);
        RIIC1.IER.UINT32 |= _RIIC_NACK_RECEPTION_INT_ENABLE;
        RIIC1.MR3.UINT32 &= (unsigned long)~_RIIC_NACK_TRANSMISSION;
    }
    if (flags & _RIIC_START_CONDITION_DETECTED)
    {
        /* A new address/dummy phase follows START. No protocol byte is
         * manufactured from the address or the dummy DRR read.
         */
        g_addressed = 0U;
        g_accepted = 0U;
        RIIC1.SR2.UINT32 &= (unsigned long)~_RIIC_START_CONDITION_DETECTED;
    }
    synced = RIIC1.SR2.UINT32;
    (void)synced;
    __syncp();
}
unsigned char IspBusInit(void)
{
    IspMailboxInit(&g_box);
    g_addressed = g_reading = g_accepted = 0U;
    g_bus_errors = 0UL;
    g_nack_seen = g_tx_loaded = 0U;
    R_Config_RIIC1_Create();
    RIIC1.SAR0.UINT32 = (unsigned long)ISP_I2C_ADDRESS << 1;
    /* Error/START/STOP runs before byte IRQs when requests are simultaneous.
     * Preserve table selection/masks and change only the priority nibble.
     */
    INTC2.ICRIIC1EE.UINT16 = (unsigned short)((INTC2.ICRIIC1EE.UINT16 & 0xFFF0U) | 14U);
    /* TEI is not used for byte transfer/completion; keep its request masked. */
    RIIC1.IER.UINT32 &= (unsigned long)~_RIIC_TRANSMIT_END_INT_ENABLE;
    R_Config_RIIC1_Start();
    INTC2.ICRIIC1TEI.BIT.MKRIIC1TEI = 1U;
    g_bus_activity = IspTick();
    return 1U;
}
void IspBusStop(void)
{
    R_Config_RIIC1_Stop();
}
unsigned char IspBusGet(unsigned char *byte)
{
    return IspMailboxGet(&g_box, byte);
}
unsigned char IspBusSend(const unsigned char *bytes, unsigned short size)
{
    unsigned char result;
    unsigned long psw;
    if (size > ISP_FRAME_MAX)
    {
        return 0U;
    }
    psw = Device_IrqSave();
    result = IspMailboxPublish(&g_box, bytes, (unsigned char)size);
    Device_IrqRestore(psw);
    return result;
}
void IspBusPoll(void)
{
    unsigned long psw;
    psw = Device_IrqSave();
    if (g_box.busy && (unsigned short)(IspTick() - g_bus_activity) >= 2000U)
    {
        IspMailboxInit(&g_box);
        g_addressed = g_reading = g_accepted = 0U;
    }
    IspMailboxRelease(&g_box);
    Device_IrqRestore(psw);
}
unsigned char IspBusReplyComplete(void)
{
    return g_box.reply_done;
}
