#ifndef F24_BRIDGE_SUPPORT_H
#define F24_BRIDGE_SUPPORT_H
#include "bridge_update_engine.h"
#include "image_metadata_config.h"
#include "boot_app_handoff.h"
#include "image_metadata_profile.h"
uint8_t F24Bridge_Read(void *context, uint32_t address);
void F24Bridge_ResetToBoot(void);
void F24Bridge_Reset(void);
void F24Bridge_Jump(void);
/* App dispatcher hook: returns zero only for commands it does not own. */
uint8_t F24Bridge_AppCommand(uint8_t command, const uint8_t *payload, uint16_t length,
                            uint8_t *reply, uint16_t *reply_length, uint8_t *status);
void F24Bridge_AppProcess(void);
void F24Bridge_AppResponseSent(void);
#endif
