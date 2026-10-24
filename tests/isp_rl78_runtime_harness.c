/* Actual project globals/config and transport loop. Only byte I/O, tick,
 * reset and physical Flash are native boundaries, not protocol substitutes.
 */
#include "isp_device.h"
#include "isp_xmodem.h"
#include "isp_i2c_mailbox.h"
#include "image_metadata_config.h"
#include "isp_transport_config.h"
#if defined(ISP_MCU_G16) || defined(ISP_MCU_G22)
#include "isp_board_config.h"
#endif
#include "boot_app_handoff.h"
#include <stdint.h>
#include <string.h>
#define __far
#undef BOOT_APP_SHARED_RAM_ADDRESS
#define BOOT_APP_SHARED_RAM_ADDRESS ((uintptr_t)test_handoff)

static unsigned short test_tick,test_rx_at,test_rx_size,test_tx_size;
static unsigned char test_rx[512],test_tx[512],test_send_ok,test_complete;
static unsigned long test_resets,test_handoff[4];
static IspI2cMailbox test_box;
unsigned short IspTick(void){return test_tick;}
void IspHeartbeat(void){}
void IspBoardReset(void){++test_resets;}
unsigned char IspUartGet(unsigned char *byte)
{
    if(test_rx_at==test_rx_size){return 0U;}
    *byte=test_rx[test_rx_at++];return 1U;
}
unsigned char IspUartSend(const unsigned char *bytes,unsigned short size)
{
    if(!test_send_ok||size>sizeof(test_tx)-test_tx_size){return 0U;}
    memcpy(test_tx+test_tx_size,bytes,size);test_tx_size+=size;return 1U;
}
void IspUartDiagnostics(unsigned char *bytes)
{
    memset(bytes,0,ISP_DIAGNOSTIC_BYTES);
    bytes[0]=1U;bytes[1]=1U;bytes[2]=15U;
    IspWrite32(bytes+4U,2UL);IspWrite32(bytes+8U,3UL);
    IspWrite32(bytes+12U,4UL);IspWrite32(bytes+16U,17UL);
}
void IspI2cPoll(unsigned short now){(void)now;}
unsigned char IspI2cReplyComplete(void){return test_box.reply_done;}
unsigned char IspI2cGet(unsigned char *byte){return IspMailboxGet(&test_box,byte);}
unsigned char IspI2cSend(const unsigned char *bytes,unsigned short size)
{
    if(!test_send_ok){return 0U;}
    return IspMailboxPublish(&test_box,bytes,(unsigned char)size);
}
void IspI2cRelease(void){IspMailboxRelease(&test_box);}
#include "actual_rl78_globals.inc"
#include "actual_rl78_transport.inc"

void RlRuntimeInit(const IspDeviceOps *ops,unsigned long version,unsigned long build,unsigned long size)
{
    memset(&g_device,0,sizeof(g_device));memset(&g_request,0,sizeof(g_request));
    memset(&g_reply,0,sizeof(g_reply));IspStreamInit(&g_stream);
#if defined(ISP_DEVICE_BOOT)
    IspDeviceInit(&g_device,&g_config,ops,ISP_MODE_BOOT);
#elif defined(ISP_DEVICE_APP)
    IspDeviceInit(&g_device,&g_config,ops,ISP_MODE_APP);
    IspDeviceInstalled(&g_device,version,build,size,0U);
#else
    IspDeviceInit(&g_device,&g_config,ops,ISP_MODE_TRANSFER);
#endif
#if !defined(ISP_DEVICE_APP)
    (void)version;(void)build;(void)size;
#endif
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
    IspDeviceSetDiagnostics(&g_device,IspUartDiagnostics);
#endif
    test_tick=test_rx_at=test_rx_size=test_tx_size=0U;
    g_last_rx=g_last_activity=0U;test_resets=0UL;test_send_ok=test_complete=1U;
    memset(test_handoff,0,sizeof(test_handoff));IspMailboxInit(&test_box);
#if defined(ISP_I2C) || defined(ISP_XMODEM)
    g_pending_action=0U;
#endif
#if defined(ISP_XMODEM)
    g_x_active=0U;memset(&g_input,0,sizeof(g_input));
#endif
}
unsigned short RlRuntimeExchange(const unsigned char *bytes,unsigned short size,
    unsigned short advance,unsigned char *output)
{
    unsigned short at;
    if(size>sizeof(test_rx)){return 0U;}
    test_tx_size=0U;test_tick=(unsigned short)(test_tick+advance);
#if defined(ISP_I2C)
    if(size){
        if(!IspMailboxStart(&test_box,0U)){return 0U;}
        for(at=0U;at<size;++at){(void)IspMailboxReceive(&test_box,bytes[at]);}
        IspMailboxStop(&test_box);
    }
    IspRuntimePoll();
    if(test_box.reply_ready){
        test_tx_size=test_box.reply_length;memcpy(test_tx,test_box.reply,test_tx_size);
        if(test_complete){
            (void)IspMailboxStart(&test_box,1U);
            for(at=0U;at<ISP_I2C_REPLY_BYTES;++at){(void)IspMailboxNext(&test_box);}
            IspMailboxNack(&test_box);IspMailboxStop(&test_box);IspRuntimePoll();
        }
    }
#else
    (void)at;
    if(test_rx_at!=test_rx_size){return 0U;}
    if(size){memcpy(test_rx,bytes,size);}
    test_rx_at=0U;test_rx_size=size;IspRuntimePoll();
#endif
    if(test_tx_size){memcpy(output,test_tx,test_tx_size);}
    return test_tx_size;
}
void RlRuntimeSendResult(unsigned char okay){test_send_ok=okay;}
void RlRuntimeCompletion(unsigned char complete){test_complete=complete;}
unsigned long RlRuntimeResets(void){return test_resets;}
unsigned char RlRuntimeActive(void){return g_device.active;}
unsigned short RlRuntimeBuffered(void){return g_stream.count;}
