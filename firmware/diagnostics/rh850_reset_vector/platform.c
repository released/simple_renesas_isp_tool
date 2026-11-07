#include "recovery.h"
#include "r_fcl.h"
#include "iodefine.h"

static const r_fcl_descriptor_t descriptor =
{
    {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL},
    0xFEDF8000UL,
    120U
};

int RvClockReady(void)
{
    /* First run the already verified Transfer from PC=0, then pause it.
     * These checks validate its 16 MHz crystal / 120 MHz CPU register profile;
     * they cannot measure an incorrectly populated physical crystal.
     */
    return CLKCTL.MOSCM == 0UL && CLKCTL.MOSCC == 6UL &&
        (CLKCTL.MOSCS & 4UL) == 4UL && CLKCTL.PLL1C == 0x0001031DUL &&
        (CLKCTL.PLL1S & 4UL) == 4UL &&
        CLKCTL.CKSC_PLL1IS_ACT == 1UL &&
        CLKCTL.CKSC_CPUCLKS_ACT == 3UL &&
        CLKCTL.CKSC_CPUCLKD_ACT == 0x11UL &&
        CLKCTL.CKSC_PPLLCLKS_ACT == 3UL;
}

unsigned long RvPrepare(void)
{
    r_fcl_status_t status;
    status = R_FCL_Init(&descriptor);
    if (status == R_FCL_OK)
    {
        status = R_FCL_CopySections();
    }
    if (status == R_FCL_OK)
    {
        return RvExecute(R_FCL_CMD_PREPARE_ENV, 0UL, 0);
    }
    return (unsigned long)status;
}

unsigned long RvExecute(r_fcl_command_t command, unsigned long index, void *buffer)
{
    r_fcl_request_t request;
    volatile unsigned long delay;

    /* A second allowlist prevents routing an erase, page write, ID, OTP,
     * option-byte or security command through this backend.
     */
    switch (command)
    {
        case R_FCL_CMD_PREPARE_ENV:
        case R_FCL_CMD_GET_DEVICE_NAME:
        case R_FCL_CMD_GET_BLOCK_CNT:
        case R_FCL_CMD_GET_BLOCK_END_ADDR:
        case R_FCL_CMD_GET_RESET_VECTOR:
        case R_FCL_CMD_GET_OPB:
        case R_FCL_CMD_SET_RESET_VECTOR:
            break;
        default:
            return (unsigned long)R_FCL_ERR_COMMAND;
    }
    request.command_enu = command;
    request.idx_u32 = index;
    request.cnt_u16 = 0U;
    request.bufferAdd_u32 = (uint32_t)buffer;
    request.status_enu = R_FCL_ERR_FLOW;
    FLMD.PCMD = 0xA5UL;
    FLMD.CNT = 1UL;
    FLMD.CNT = ~1UL;
    FLMD.CNT = 1UL;
    for (delay = 0UL; delay < 10000UL; delay++)
    {
        /* Match the existing SDK stabilization delay without an unbounded retry. */
    }
    R_FCL_Execute(&request);
    FLMD.PCMD = 0xA5UL;
    FLMD.CNT = 0UL;
    FLMD.CNT = ~0UL;
    FLMD.CNT = 0UL;
    for (delay = 0UL; delay < 10000UL; delay++)
    {
        /* Keep access disabled before the operator inspects the mailbox. */
    }
    return (unsigned long)request.status_enu;
}
