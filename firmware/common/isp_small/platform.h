#ifndef ISP_SMALL_PLATFORM_H
#define ISP_SMALL_PLATFORM_H
#include "iodefine.h"
#include "isp_board_config.h"
#define BSP_DI() __DI()
#define BSP_EI() __EI()
#define BSP_NOP() __nop()
unsigned char Device_FlashInit(void);
unsigned char Device_FlashErase(unsigned long address);
unsigned char Device_FlashWrite(unsigned long address, unsigned char __near *data);
#endif
