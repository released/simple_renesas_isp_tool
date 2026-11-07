#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recovery.h"

static unsigned long stored[4];
static unsigned long original[4];
static unsigned long options[8];
static unsigned long observed_rbase;
static unsigned long prepare_status;
static int clock_ready;
static int prepare_calls;
static int calls;
static int fail_at;
static int writes;
static int reads;
static int change_fresh;
static int bad_readback;
static int changed_options;
static int wrong_name;
static unsigned long blocks;
static unsigned long last_end;
static unsigned int checks;

static void Check(int condition, const char *name)
{
    checks++;
    if (!condition)
    {
        fprintf(stderr, "FAIL %s (phase=%lu error=%lu calls=%d writes=%d)\n",
                name, g_rv.phase, g_rv.error, calls, writes);
        exit(1);
    }
}

static void ResetModel(void)
{
    unsigned int index;
    memset((void *)&g_rv, 0, sizeof(g_rv));
    stored[0] = RV_EXPECTED_VECTOR;
    stored[1] = 0x12345678UL;
    stored[2] = 0xA5A55A5AUL;
    stored[3] = 0xFEEDC0DEUL;
    memcpy(original, stored, sizeof(stored));
    for (index = 0U; index < 8U; index++)
    {
        options[index] = 0xABCD0010UL + index;
    }
    observed_rbase = RV_EXPECTED_VECTOR;
    prepare_status = R_FCL_OK;
    clock_ready = 1;
    prepare_calls = 0;
    calls = 0;
    fail_at = -1;
    writes = 0;
    reads = 0;
    change_fresh = 0;
    bad_readback = 0;
    changed_options = 0;
    wrong_name = 0;
    blocks = 38UL;
    last_end = 0x000FFFFFUL;
}

unsigned long RvReadRbase(void)
{
    return observed_rbase;
}
int RvClockReady(void)
{
    return clock_ready;
}
unsigned long RvPrepare(void)
{
    prepare_calls++;
    return prepare_status;
}

unsigned long RvExecute(r_fcl_command_t command, unsigned long index, void *buffer)
{
    int current;
    current = calls++;
    if (command == R_FCL_CMD_SET_RESET_VECTOR)
    {
        writes++;
    }
    if (current == fail_at)
    {
        return R_FCL_ERR_INTERNAL;
    }
    switch (command)
    {
        case R_FCL_CMD_GET_DEVICE_NAME:
            memset(buffer, 0, 16U);
            memcpy(buffer, wrong_name ? "R7F701692" : "R7F701684", 9U);
            break;
        case R_FCL_CMD_GET_BLOCK_CNT:
            *(unsigned long *)buffer = blocks;
            break;
        case R_FCL_CMD_GET_BLOCK_END_ADDR:
            Check(index == 37UL, "actual final block query");
            *(unsigned long *)buffer = last_end;
            break;
        case R_FCL_CMD_GET_RESET_VECTOR:
            reads++;
            if (change_fresh && reads == 2)
            {
                stored[1] ^= 1UL;
            }
            memcpy(buffer, stored, sizeof(stored));
            if (writes && bad_readback)
            {
                ((unsigned long *)buffer)[3] ^= 1UL;
            }
            break;
        case R_FCL_CMD_GET_OPB:
            memcpy(buffer, options, sizeof(options));
            if (writes && changed_options)
            {
                ((unsigned long *)buffer)[7] ^= 1UL;
            }
            break;
        case R_FCL_CMD_SET_RESET_VECTOR:
            Check(((unsigned long *)buffer)[0] == 0UL, "only zero destination");
            Check(memcmp((unsigned long *)buffer + 1, original + 1, 12U) == 0,
                  "all twelve non-vector bytes preserved");
            memcpy(stored, buffer, sizeof(stored));
            break;
        default:
            Check(0, "no other write/erase/security command");
            break;
    }
    return R_FCL_OK;
}

static void Arm(void)
{
    g_rv.confirm_before = RV_EXPECTED_VECTOR;
    g_rv.command = RV_WRITE_COMMAND;
    RvPoll();
}

static void TerminalDoesNotRetry(void)
{
    int old_calls;
    int old_writes;
    unsigned int index;
    old_calls = calls;
    old_writes = writes;
    for (index = 0U; index < 20U; index++)
    {
        Arm();
    }
    Check(calls == old_calls && writes == old_writes, "terminal state never retries");
}

int main(void)
{
    unsigned int index;
    int failure;
    Check(offsetof(RvMailbox, before) == 0x30U, "before ABI");
    Check(offsetof(RvMailbox, options_before) == 0x60U, "32-byte OPB ABI");
    Check(offsetof(RvMailbox, trap_pc) == 0xB0U, "trap ABI");
    Check(sizeof(RvMailbox) == 0xB8U, "fixed mailbox extent");

    ResetModel();
    g_rv.command = RV_WRITE_COMMAND;
    g_rv.confirm_before = RV_EXPECTED_VECTOR;
    RvBegin();
    for (index = 0U; index < 100U; index++)
    {
        RvPoll();
    }
    Check(g_rv.phase == RV_PHASE_READY && writes == 0, "startup ignores stale arm");
    Check(memcmp((const void *)g_rv.before, original, 16U) == 0, "complete original snapshot");
    Check(memcmp((const void *)g_rv.options_before, options, 32U) == 0, "complete OPB snapshot");
    g_rv.desired[1] ^= 1UL;
    Arm();
    Check(g_rv.phase == RV_PHASE_VERIFIED && writes == 1, "one successful SET and verify");
    Check(g_rv.write_attempts == 1UL && g_rv.command == 0UL, "request consumed");
    Check(memcmp((const void *)g_rv.options_before, (const void *)g_rv.options_after, 32U) == 0,
          "options unchanged");
    TerminalDoesNotRetry();

    ResetModel();
    clock_ready = 0;
    RvBegin();
    Check(g_rv.error == RV_ERROR_CLOCK && prepare_calls == 0 && calls == 0, "wrong clock before FCL");
    TerminalDoesNotRetry();
    ResetModel();
    prepare_status = R_FCL_ERR_FLOW;
    RvBegin();
    Check(g_rv.error == RV_ERROR_PREPARE && calls == 0, "prepare error");
    ResetModel();
    wrong_name = 1;
    RvBegin();
    Check(g_rv.error == RV_ERROR_DEVICE && writes == 0, "wrong silicon");
    ResetModel();
    blocks = 70UL;
    RvBegin();
    Check(g_rv.error == RV_ERROR_GEOMETRY && writes == 0, "wrong block count");
    ResetModel();
    last_end = 0x1FFFFFUL;
    RvBegin();
    Check(g_rv.error == RV_ERROR_GEOMETRY && writes == 0, "wrong capacity");
    ResetModel();
    stored[0] = 0UL;
    RvBegin();
    Check(g_rv.phase == RV_PHASE_ALREADY_ZERO && writes == 0, "already zero never writes");
    TerminalDoesNotRetry();
    ResetModel();
    stored[0] = 0x8000UL;
    RvBegin();
    Check(g_rv.error == RV_ERROR_VECTOR && writes == 0, "unknown persistent vector");
    ResetModel();
    observed_rbase = 0UL;
    RvBegin();
    Check(g_rv.error == RV_ERROR_VECTOR && writes == 0, "live / stored mismatch");
    ResetModel();
    RvBegin();
    g_rv.command = RV_WRITE_COMMAND;
    RvPoll();
    Check(g_rv.error == RV_ERROR_CONFIRM && writes == 0, "missing confirmation");
    TerminalDoesNotRetry();
    ResetModel();
    RvBegin();
    g_rv.confirm_before = RV_EXPECTED_VECTOR;
    g_rv.command = 1UL;
    RvPoll();
    Check(g_rv.error == RV_ERROR_CONFIRM && writes == 0, "wrong command");
    ResetModel();
    RvBegin();
    clock_ready = 0;
    Arm();
    Check(g_rv.error == RV_ERROR_CLOCK && writes == 0, "clock changed while paused");
    ResetModel();
    RvBegin();
    observed_rbase = 0UL;
    Arm();
    Check(g_rv.error == RV_ERROR_VECTOR && writes == 0, "RBASE changed while paused");
    ResetModel();
    RvBegin();
    change_fresh = 1;
    Arm();
    Check(g_rv.error == RV_ERROR_CHANGED && writes == 0, "fresh reserved bytes changed");
    ResetModel();
    RvBegin();
    options[7] ^= 1UL;
    Arm();
    Check(g_rv.error == RV_ERROR_CHANGED && writes == 0, "fresh options changed");
    ResetModel();
    RvBegin();
    g_rv.before[2] ^= 1UL;
    Arm();
    Check(g_rv.error == RV_ERROR_CHANGED && writes == 0, "edited original snapshot rejected");
    ResetModel();
    RvBegin();
    bad_readback = 1;
    Arm();
    Check(g_rv.error == RV_ERROR_VERIFY && writes == 1, "all sixteen SET bytes verified");
    TerminalDoesNotRetry();
    ResetModel();
    RvBegin();
    changed_options = 1;
    Arm();
    Check(g_rv.error == RV_ERROR_OPTIONS && writes == 1, "last OPB word checked");
    TerminalDoesNotRetry();
    for (failure = 0; failure < 10; failure++)
    {
        ResetModel();
        fail_at = failure;
        RvBegin();
        if (g_rv.phase == RV_PHASE_READY)
        {
            Arm();
        }
        Check(g_rv.phase == RV_PHASE_FAILED, "every vendor error is terminal");
        Check(writes <= 1, "no repeated SET on vendor error");
        if (failure == 7)
        {
            Check(g_rv.error == RV_ERROR_WRITE && calls == 8 && reads == 2,
                  "failed SET never followed by unreliable GET");
        }
        TerminalDoesNotRetry();
    }
    printf("PASS: %u checks; actual recovery state machine; hardware NOT RUN.\n", checks);
    return 0;
}
