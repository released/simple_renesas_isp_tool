#ifndef ISP_RH850_H
#define ISP_RH850_H
#include "r_cg_macrodriver.h"
#include "isp_device.h"
#include "image_metadata.h"
#include "image_metadata_config.h"
#include "isp_transport_config.h"
#include "isp_bus_contract.h"
#include "boot_app_handoff.h"
#include "device_image.h"

/* The portable protocol has no register/Flash dependency. These adapters own
 * the selected EVM peripheral, ECC-safe reads and RAM-executed FCL calls.
 */
unsigned char Device_ReadPrepare(void);
unsigned char Device_FlashRead(unsigned long address, unsigned char *out, unsigned short size);
unsigned char Device_FlashInit(void);
unsigned char Device_FlashErase(unsigned long address);
unsigned char Device_FlashWritePage(unsigned long address, const unsigned char *data);
unsigned long Device_IrqSave(void);
void Device_IrqRestore(unsigned long psw);
void Device_InstructionSync(void);
unsigned long Device_ReadRomStart(void);
unsigned long Device_ReadRomSize(void);
void Device_JumpAsm(void);
void Device_RequestReset(unsigned long request);
unsigned short IspTick(void);
unsigned char IspBusInit(void);
void IspBusStop(void);
unsigned char IspBusGet(unsigned char *byte);
unsigned char IspBusSend(const unsigned char *bytes, unsigned short size);
void IspBusPoll(void);
unsigned char IspBusReplyComplete(void);
void IspBusDiagnostics(unsigned char *bytes);
void IspUartRxIsr(void);
void IspUartErrorIsr(void);
void IspI2cTxIsr(void);
void IspI2cRxIsr(void);
void IspI2cEventIsr(void);
void IspI2cEndIsr(void);
#endif
