#include "recovery.h"

#ifndef RV_NATIVE_TEST
#pragma section r0_disp32 "RV_MAILBOX"
#endif
volatile RvMailbox g_rv;
#ifndef RV_NATIVE_TEST
#pragma section default
#endif

static void Fail(unsigned long error)
{
    g_rv.error = error;
    g_rv.phase = RV_PHASE_FAILED;
}

static int Execute(r_fcl_command_t command, unsigned long index, void *buffer)
{
    g_rv.last_command = (unsigned long)command;
    g_rv.fcl_status = RvExecute(command, index, buffer);
    return g_rv.fcl_status == (unsigned long)R_FCL_OK;
}

static int Equal(const volatile unsigned long *left,
                 const volatile unsigned long *right, unsigned int count)
{
    unsigned int index;
    for (index = 0U; index < count; index++)
    {
        if (left[index] != right[index])
        {
            return 0;
        }
    }
    return 1;
}

void RvBegin(void)
{
    unsigned int index;
    static const unsigned char expected_name[9] =
        {'R', '7', 'F', '7', '0', '1', '6', '8', '4'};

    /* Entry clears our BSS before this call. Ignore any stale debugger request. */
    g_rv.command = 0UL;
    g_rv.confirm_before = 0UL;
    g_rv.write_attempts = 0UL;
    g_rv.magic = RV_MAGIC;
    g_rv.phase = 1UL;
    g_rv.rbase = RvReadRbase();
    if (!RvClockReady())
    {
        Fail(RV_ERROR_CLOCK);
        return;
    }
    g_rv.phase = 2UL;
    g_rv.fcl_status = RvPrepare();
    if (g_rv.fcl_status != (unsigned long)R_FCL_OK)
    {
        Fail(RV_ERROR_PREPARE);
        return;
    }
    if (!Execute(R_FCL_CMD_GET_DEVICE_NAME, 0UL, (void *)g_rv.device_name) ||
        !Execute(R_FCL_CMD_GET_BLOCK_CNT, 0UL, (void *)&g_rv.block_count))
    {
        Fail(RV_ERROR_COMMAND);
        return;
    }
    for (index = 0U; index < 9U; index++)
    {
        if (g_rv.device_name[index] != expected_name[index])
        {
            Fail(RV_ERROR_DEVICE);
            return;
        }
    }
    if (g_rv.block_count != 38UL)
    {
        Fail(RV_ERROR_GEOMETRY);
        return;
    }
    if (!Execute(R_FCL_CMD_GET_BLOCK_END_ADDR, 37UL,
                 (void *)&g_rv.last_block_end))
    {
        Fail(RV_ERROR_COMMAND);
        return;
    }
    if (g_rv.last_block_end != 0x000FFFFFUL)
    {
        Fail(RV_ERROR_GEOMETRY);
        return;
    }
    if (!Execute(R_FCL_CMD_GET_RESET_VECTOR, 0UL, (void *)g_rv.before) ||
        !Execute(R_FCL_CMD_GET_OPB, 0UL, (void *)g_rv.options_before))
    {
        Fail(RV_ERROR_COMMAND);
        return;
    }
    for (index = 0U; index < 4U; index++)
    {
        g_rv.desired[index] = g_rv.before[index];
    }
    g_rv.desired[0] = 0UL;
    if (g_rv.before[0] == 0UL)
    {
        g_rv.phase = RV_PHASE_ALREADY_ZERO;
        return;
    }
    /* Do not guess how to repair another value or a different buffer layout. */
    if (g_rv.before[0] != RV_EXPECTED_VECTOR ||
        (g_rv.rbase & 0xFFFFFE00UL) != RV_EXPECTED_VECTOR)
    {
        Fail(RV_ERROR_VECTOR);
        return;
    }
    g_rv.phase = RV_PHASE_READY;
}

void RvPoll(void)
{
    unsigned long request;
    unsigned long confirmation;
    unsigned long fresh[4];
    unsigned long options[8];
    unsigned int index;

    g_rv.heartbeat++;
    if (g_rv.phase != RV_PHASE_READY || g_rv.command == 0UL)
    {
        return;
    }
    request = g_rv.command;
    confirmation = g_rv.confirm_before;
    /* Consume once before any call which can change persistent configuration. */
    g_rv.command = 0UL;
    g_rv.confirm_before = 0UL;
    if (request != RV_WRITE_COMMAND || confirmation != RV_EXPECTED_VECTOR ||
        g_rv.write_attempts != 0UL)
    {
        Fail(RV_ERROR_CONFIRM);
        return;
    }
    if (!RvClockReady())
    {
        Fail(RV_ERROR_CLOCK);
        return;
    }
    if ((RvReadRbase() & 0xFFFFFE00UL) != RV_EXPECTED_VECTOR)
    {
        Fail(RV_ERROR_VECTOR);
        return;
    }
    if (!Execute(R_FCL_CMD_GET_RESET_VECTOR, 0UL, fresh) ||
        !Execute(R_FCL_CMD_GET_OPB, 0UL, options))
    {
        Fail(RV_ERROR_COMMAND);
        return;
    }
    if (!Equal(fresh, g_rv.before, 4U) ||
        !Equal(options, g_rv.options_before, 8U) ||
        fresh[0] != RV_EXPECTED_VECTOR)
    {
        Fail(RV_ERROR_CHANGED);
        return;
    }
    /* Rebuild from fresh bytes; never trust an edited debugger output buffer. */
    for (index = 0U; index < 4U; index++)
    {
        g_rv.desired[index] = fresh[index];
    }
    g_rv.desired[0] = 0UL;
    g_rv.phase = RV_PHASE_WRITING;
    g_rv.write_attempts = 1UL;
    if (!Execute(R_FCL_CMD_SET_RESET_VECTOR, 0UL, (void *)g_rv.desired))
    {
        /* The vendor manual forbids trusting GET after a failed SET. */
        Fail(RV_ERROR_WRITE);
        return;
    }
    g_rv.phase = RV_PHASE_VERIFYING;
    if (!Execute(R_FCL_CMD_GET_RESET_VECTOR, 0UL, (void *)g_rv.after) ||
        !Execute(R_FCL_CMD_GET_OPB, 0UL, (void *)g_rv.options_after))
    {
        Fail(RV_ERROR_VERIFY);
        return;
    }
    if (!Equal(g_rv.after, g_rv.desired, 4U))
    {
        Fail(RV_ERROR_VERIFY);
        return;
    }
    if (!Equal(g_rv.options_before, g_rv.options_after, 8U))
    {
        Fail(RV_ERROR_OPTIONS);
        return;
    }
    g_rv.phase = RV_PHASE_VERIFIED;
}

void RvMain(void)
{
    RvBegin();
    for (;;)
    {
        RvPoll();
    }
}
