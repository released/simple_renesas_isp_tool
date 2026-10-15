#include "platform.h"
#include "f24_bridge_support.h"
#include "r_rfd_common_api.h"
#include "r_rfd_common_control_api.h"
#include "r_rfd_common_get_api.h"
#include "r_rfd_code_flash_api.h"

/* Every function executing while code flash is in programming mode is linked
 * to RAM. This includes this driver, all RFD callees and critical-section
 * hooks. Interrupts are disabled before entry. No code/constant/IRQ fetch may
 * come from code flash until NONPROGRAMMABLE has been confirmed.
 * Timeout forces a reset from RAM, never returns into unavailable code flash. */
#pragma section text BRAM
static void __far fail_reset(void) {
    *(volatile uint32_t __far *)F24_BRIDGE_COOKIE_ADDRESS=F24_BRIDGE_BOOT_COOKIE;
    IAWCTL|=0x80U; *(volatile uint8_t __far *)0UL=0U;
    for (;;) { BSP_NOP(); }
}
static uint8_t __far wait_end(void) {
    uint32_t budget;
    uint8_t error;
    budget=2000000UL;
    while (R_RFD_CheckCFDFSeqEndStep1()==R_RFD_ENUM_RET_STS_BUSY)
        if (!--budget) fail_reset();
    budget=2000000UL;
    while (R_RFD_CheckCFDFSeqEndStep2()==R_RFD_ENUM_RET_STS_BUSY)
        if (!--budget) fail_reset();
    R_RFD_GetSeqErrorStatus(&error); R_RFD_ClearSeqRegister();
    return error==0U;
}
static uint8_t __far execute(uint32_t address, const uint8_t *data, uint8_t erase) {
    uint8_t result, bytes[4];
    e_rfd_ret_t mode;
    result=0U;
    /* Copy the write source into near RAM before changing the flash mode. */
    if (!erase) { bytes[0]=data[0]; bytes[1]=data[1]; bytes[2]=data[2]; bytes[3]=data[3]; }
    mode=R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_CODE_PROGRAMMING);
    if (mode==R_RFD_ENUM_RET_STS_OK) {
        if (erase) R_RFD_EraseCodeFlashReq((uint16_t)(address>>10));
        else R_RFD_WriteCodeFlashReq(address,bytes);
        result=wait_end();
    }
    mode=R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_CODE_TO_NONPROGRAMMABLE);
    if (mode!=R_RFD_ENUM_RET_STS_OK ||
        R_RFD_CheckFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_CODE_TO_NONPROGRAMMABLE)!=R_RFD_ENUM_RET_STS_OK)
        fail_reset();
    return result;
}
#pragma section
static uint8_t erase_block(void *context, uint32_t address) {
    uint8_t result;
    (void)context;
    if (address<F24_BRIDGE_APP_START || address>=F24_BRIDGE_APP_LIMIT ||
        address%F24_BRIDGE_ERASE_SIZE) return 0U;
    BSP_DI(); result=execute(address,0,1U); BSP_EI(); return result;
}
static uint8_t write_four(void *context, uint32_t address, const uint8_t *data) {
    uint8_t result;
    (void)context;
    if (address<F24_BRIDGE_APP_START || address>F24_BRIDGE_APP_LIMIT-4UL || address%4UL) return 0U;
    BSP_DI(); result=execute(address,data,0U); BSP_EI(); return result;
}
uint8_t F24Flash_Init(bu_flash_t *flash) {
    flash->context=0; flash->read=F24Bridge_Read; flash->erase=erase_block; flash->write4=write_four;
    return R_RFD_Init(40U)==R_RFD_ENUM_RET_STS_OK;
}
