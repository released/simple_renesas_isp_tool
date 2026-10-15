#ifndef BRIDGE_BOOT_LINK_H
#define BRIDGE_BOOT_LINK_H
#include "bridge_update_engine.h"
#include "host_link_limits.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BU_LINK_FRAME_SIZE HOST_WIRE_MAX_FRAME
typedef struct {
    bu_engine_t *engine;
    uint8_t input[BU_LINK_FRAME_SIZE], reply[BU_LINK_FRAME_SIZE];
    uint8_t last_request[BU_LINK_FRAME_SIZE], last_reply[BU_LINK_FRAME_SIZE];
    uint16_t count, reply_length, last_length, last_reply_length;
    uint8_t action;
} bu_link_t;
/* Action: 0 none, 1 reset, 2 run. Execute only after reply physically drains. */
void Bu_LinkInit(bu_link_t *link, bu_engine_t *engine);
void Bu_LinkFeed(bu_link_t *link, uint8_t byte);
void Bu_LinkTimeout(bu_link_t *link);
#ifdef __cplusplus
}
#endif
#endif
