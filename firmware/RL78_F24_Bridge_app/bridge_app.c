#include "platform.h"
#include "host_link_uart.h"
#include "f24_bridge_support.h"
static bu_engine_t g_app_inspection;
static uint8_t g_info_ready, g_enter_pending;
uint8_t F24Bridge_AppCommand(uint8_t command, const uint8_t *p, uint16_t length,
                            uint8_t *reply, uint16_t *reply_length, uint8_t *status) {
    bu_info_t info;
    bu_flash_t flash;
    if (command!=BU_INFO_COMMAND && command!=BU_ENTER_COMMAND && command!=BU_READ_METADATA_COMMAND &&
        command!=BU_DIAGNOSTICS_COMMAND) return 0U;
    *reply_length=0U; *status=BU_OK;
    if (command==BU_INFO_COMMAND || command==BU_READ_METADATA_COMMAND || command==BU_DIAGNOSTICS_COMMAND) {
        if (command!=BU_READ_METADATA_COMMAND && length) { *status=BU_BAD_ARGUMENT; return 1U; }
        if (!g_info_ready) {
            F24Bridge_Info(&info,BU_MODE_APP);
            flash.context=0; flash.read=F24Bridge_Read; flash.erase=0; flash.write4=0;
            if (!Bu_EngineInit(&g_app_inspection,&info,&flash)) { *status=BU_BAD_STATE; return 1U; }
            Bu_ValidateInstalled(&g_app_inspection); g_info_ready=1U;
        }
        if (command==BU_READ_METADATA_COMMAND)
            *status=Bu_ReadMetadata(&g_app_inspection,p,length,reply,reply_length);
        else if (command==BU_DIAGNOSTICS_COMMAND) {
            reply[0]=1U; reply[1]=g_app_inspection.hold_reason; reply[2]=0U;
            reply[3]=g_app_inspection.info.flags; Bu_Write32(reply+4U,0UL); *reply_length=8U;
        } else { Bu_EncodeInfo(&g_app_inspection.info,reply); *reply_length=BU_INFO_SIZE; }
    } else if (length!=4U || Bu_Read16(p)!=F24_BRIDGE_BOARD_ID || p[2]!=BU_PROTOCOL || p[3]!=0xB0U)
        *status=BU_BAD_ARGUMENT;
    else g_enter_pending=1U;
    return 1U;
}
void F24Bridge_AppProcess(void) {
    if (g_enter_pending==2U && HostLinkUart_IsTxIdle()) F24Bridge_ResetToBoot();
}
void F24Bridge_AppResponseSent(void) { if (g_enter_pending==1U) g_enter_pending=2U; }
