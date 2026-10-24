/* Execute the actual RuntimePoll, mocking only byte I/O, ticks and reset. */
#include "isp_device.h"
#if defined(ISP_XMODEM)
#include "isp_xmodem.h"
#endif
#include "device_image.h"
#include "image_metadata.h"
#include "image_metadata_config.h"
#include "isp_transport_config.h"
#include "boot_app_handoff.h"
#include <string.h>

static unsigned short test_tick;
static unsigned char test_rx[512],test_tx[512];
static unsigned short test_rx_size,test_rx_at,test_tx_size;
static unsigned long test_resets,test_request;
static unsigned char test_complete,test_send_ok;
unsigned short IspTick(void){return test_tick;}
unsigned char IspBusGet(unsigned char *byte)
{
    if(test_rx_at==test_rx_size){return 0U;}
    *byte=test_rx[test_rx_at++];return 1U;
}
unsigned char IspBusSend(const unsigned char *bytes,unsigned short size)
{
    if(!test_send_ok||size>sizeof(test_tx)-test_tx_size){return 0U;}
    memcpy(test_tx+test_tx_size,bytes,size);test_tx_size+=size;return 1U;
}
void IspBusPoll(void){}
unsigned char IspBusReplyComplete(void){return test_complete;}
void Device_RequestReset(unsigned long request){++test_resets;test_request=request;}
void IspBusDiagnostics(unsigned char *bytes)
{
    memset(bytes,0,ISP_DIAGNOSTIC_BYTES);bytes[0]=1U;bytes[1]=1U;bytes[2]=15U;
    IspWrite32(bytes+4U,2UL);IspWrite32(bytes+8U,3UL);IspWrite32(bytes+16U,7UL);
}
unsigned char Device_FlashInit(void);
unsigned char Device_FlashErase(unsigned long address);
unsigned char Device_FlashWritePage(unsigned long address,const unsigned char *bytes);
unsigned char Device_FlashRead(unsigned long address,unsigned char *bytes,unsigned short size);
#include "actual_runtime.inc"

void RhRuntimeInit(unsigned long version,unsigned long build,unsigned long size)
{
    memset(&g_device,0,sizeof(g_device));memset(&g_request,0,sizeof(g_request));
    memset(&g_reply,0,sizeof(g_reply));IspStreamInit(&g_stream);
#if defined(ISP_DEVICE_BOOT)
    IspDeviceInit(&g_device,&g_config,&g_ops,ISP_MODE_BOOT);
    g_flash_ready=0U;g_page_used=0U;
#elif defined(ISP_DEVICE_APP)
    IspDeviceInit(&g_device,&g_config,&g_ops,ISP_MODE_APP);
#else
    IspDeviceInit(&g_device,&g_config,&g_ops,ISP_MODE_TRANSFER);
#endif
#if !defined(ISP_DEVICE_BOOT) && !defined(ISP_I2C)
    IspDeviceSetDiagnostics(&g_device,IspBusDiagnostics);
#endif
#if !defined(ISP_DEVICE_TRANSFER)
    if(size){IspDeviceInstalled(&g_device,version,build,size,0U);}
#else
    (void)version;(void)build;(void)size;
#endif
    g_pending=0U;g_last_rx=0U;test_tick=0U;
#if defined(ISP_DEVICE_BOOT)
    g_last_activity=0U;
#endif
#if defined(ISP_XMODEM)
    memset(&g_x,0,sizeof(g_x));g_x_active=0U;
#endif
    test_rx_at=test_rx_size=test_tx_size=0U;test_resets=test_request=0UL;
    test_complete=test_send_ok=1U;
}
unsigned short RhRuntimeExchange(const unsigned char *bytes,unsigned short size,
    unsigned short advance,unsigned char *reply)
{
    if(size>sizeof(test_rx)||test_rx_at!=test_rx_size){return 0U;}
    if(size){memcpy(test_rx,bytes,size);}
    test_rx_at=0U;test_rx_size=size;test_tx_size=0U;
    test_tick=(unsigned short)(test_tick+advance);RuntimePoll();
    if(test_tx_size){memcpy(reply,test_tx,test_tx_size);}
    return test_tx_size;
}
void RhRuntimeCompletion(unsigned char complete){test_complete=complete;}
void RhRuntimeSendResult(unsigned char okay){test_send_ok=okay;}
unsigned long RhRuntimeResets(void){return test_resets;}
unsigned long RhRuntimeResetRequest(void){return test_request;}
unsigned long RhRuntimeWritten(void){return g_device.written;}
unsigned short RhRuntimeBuffered(void){return g_stream.count;}
unsigned char RhRuntimeActive(void){return g_device.active;}
unsigned char RhRuntimeReady(void){return g_device.ready;}
unsigned char RhRuntimeStreaming(void)
{
#if defined(ISP_XMODEM)
    return g_x_active;
#else
    return 0U;
#endif
}
