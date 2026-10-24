#ifndef ISP_XMODEM_RUNTIME_HARNESS_H
#define ISP_XMODEM_RUNTIME_HARNESS_H
#include "isp_device.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    unsigned long written, resets, heartbeats;
    unsigned char active, streaming, pending, ready;
} IspRuntimeSnapshot;
void IspRuntimeTestInit(const IspDeviceConfig *config, const IspDeviceOps *ops);
unsigned short IspRuntimeTestExchange(const unsigned char *input,
    unsigned short size, unsigned short advance, unsigned char *output);
IspRuntimeSnapshot IspRuntimeTestSnapshot(void);
void IspRuntimeTestSendResult(unsigned char okay);
#ifdef __cplusplus
}
#endif
#endif
