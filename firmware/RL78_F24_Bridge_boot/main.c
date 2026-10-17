#include "platform.h"
#include "r_cg_macrodriver.h"
#include "Config_UART0.h"
#include "Config_TAU0_1.h"
#include "host_link_uart.h"
#include "bridge_boot_link.h"
#include "f24_bridge_support.h"
extern unsigned int get_tick(void);
extern uint8_t F24Flash_Init(bu_flash_t *flash);
static bu_engine_t g_engine;
static bu_link_t g_link;
void main(void) {
    bu_info_t info;
    bu_flash_t flash;
    uint32_t request;
    uint16_t last_byte;
    uint8_t byte, ready;
    request=*(volatile uint32_t __far *)F24_BRIDGE_COOKIE_ADDRESS;
    *(volatile uint32_t __far *)F24_BRIDGE_COOKIE_ADDRESS=0UL;
    F24Bridge_Info(&info,BU_MODE_BOOT);
    ready=F24Flash_Init(&flash);
    if (ready) info.flags|=BU_FLAG_FLASH_READY;
    if (!Bu_EngineInit(&g_engine,&info,&flash)) F24Bridge_ResetToBoot();
    Bu_ValidateInstalled(&g_engine);
    /* SW1 forces recovery. No valid CRC => always stay in boot. */
    if ((g_engine.info.flags&BU_FLAG_VALID_APP) && request!=F24_BRIDGE_BOOT_COOKIE && (P3&0x04U))
        F24Bridge_Jump();
    if (g_engine.info.flags&BU_FLAG_VALID_APP)
        g_engine.hold_reason=((P3&0x04U)==0U) ? BU_HOLD_RECOVERY : BU_HOLD_ENTER;
    if (!ready) g_engine.hold_reason=BU_HOLD_FLASH;
    P1|=0x04U; PM1&=0xFBU; /* CAN transceiver standby. */
    P8=(uint8_t)((P8&0xF8U)|0x01U); PM8&=0xF8U;
    HostLinkUart_Init(); Bu_LinkInit(&g_link,&g_engine);
    R_Config_UART0_Start(); R_Config_TAU0_1_Start(); BSP_EI(); last_byte=get_tick();
    for (;;) {
        HostLinkUart_Process();
        if (g_link.reply_length && HostLinkUart_GetTxFree()>=g_link.reply_length) {
            if (HostLinkUart_Write(g_link.reply,g_link.reply_length)) g_link.reply_length=0U;
        }
        if (g_link.action && !g_link.reply_length && HostLinkUart_IsTxIdle()) {
            if (g_link.action==2U) F24Bridge_Jump();
            F24Bridge_ResetToBoot();
        }
        if ((uint16_t)(get_tick()-last_byte)>500U) Bu_LinkTimeout(&g_link);
        if (!g_link.reply_length && !g_link.action && HostLinkUart_Read(&byte,1U)) {
            last_byte=get_tick(); Bu_LinkFeed(&g_link,byte);
        }
        P8=(uint8_t)((P8&0xF8U)|((get_tick()&0x0200U) ? 0x01U : 0x02U));
    }
}
