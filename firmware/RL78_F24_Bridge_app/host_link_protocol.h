#ifndef HOST_LINK_PROTOCOL_H
#define HOST_LINK_PROTOCOL_H

#include <stdint.h>
#include "../common/host_link_limits.h"

#define HOST_LINK_VERSION              (0x02U)
#define HOST_LINK_MAX_PAYLOAD          HOST_WIRE_MAX_PAYLOAD
#define HOST_LINK_MAX_FRAME_SIZE       HOST_WIRE_MAX_FRAME

void HostLink_Init(void);
void HostLink_Process(void);
uint16_t HostLink_Crc16CcittFalse(const uint8_t *data, uint16_t length);

#endif
