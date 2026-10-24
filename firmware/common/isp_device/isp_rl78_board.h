#ifndef ISP_RL78_BOARD_H
#define ISP_RL78_BOARD_H

#include "isp_device.h"
#include "image_metadata.h"
#include "image_metadata_config.h"
#include "isp_transport_config.h"
#include "isp_bus_contract.h"
#if defined(ISP_I2C)
#include "isp_i2c.h"
#endif
#include "boot_app_handoff.h"

unsigned char IspBoardInit(void);
unsigned short IspTick(void);
void IspHeartbeat(void);
unsigned char IspUartGet(unsigned char *value);
unsigned char IspUartSend(const unsigned char *data, unsigned short size);
void IspUartDiagnostics(unsigned char *bytes);
void IspUartStop(void);
void IspTimerStop(void);
void IspBoardReset(void);
void IspLog(const char __far *text);
void IspRuntimeInit(unsigned char mode);
void IspRuntimePoll(void);
unsigned char IspInstalledValid(void);
unsigned char IspRuntimeActive(void);
unsigned short IspRuntimeIdle(void);
void IspRuntimeHandoff(void);
void IspUartRxIsr(void);
void IspTimerIsr(void);

#endif
