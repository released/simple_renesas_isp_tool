#ifndef RH850_RESET_VECTOR_RECOVERY_H
#define RH850_RESET_VECTOR_RECOVERY_H

#include "r_typedefs.h"
#include "fcl_cfg.h"
#include "r_fcl_types.h"

/* This diagnostic deliberately accepts only the observed R7F701684 fault. */
#define RV_MAILBOX_ADDRESS 0xFEDF7000UL
#define RV_EXPECTED_VECTOR 0x00028000UL
#define RV_WRITE_COMMAND 0x52535430UL
#define RV_MAGIC 0x52563031UL
#define RV_PHASE_READY 0x10UL
#define RV_PHASE_ALREADY_ZERO 0x11UL
#define RV_PHASE_WRITING 0x20UL
#define RV_PHASE_VERIFYING 0x30UL
#define RV_PHASE_VERIFIED 0x40UL
#define RV_PHASE_FAILED 0xE0UL
#define RV_PHASE_TRAP 0xF0UL

#define RV_ERROR_CLOCK 1UL
#define RV_ERROR_PREPARE 2UL
#define RV_ERROR_COMMAND 3UL
#define RV_ERROR_DEVICE 4UL
#define RV_ERROR_GEOMETRY 5UL
#define RV_ERROR_VECTOR 6UL
#define RV_ERROR_CONFIRM 7UL
#define RV_ERROR_CHANGED 8UL
#define RV_ERROR_WRITE 9UL
#define RV_ERROR_VERIFY 10UL
#define RV_ERROR_OPTIONS 11UL

typedef struct
{
    unsigned long magic;                 /* +00: RV01 */
    unsigned long phase;                 /* +04: READY, VERIFIED or terminal failure */
    unsigned long error;                 /* +08: RV_ERROR_* */
    unsigned long fcl_status;            /* +0C: actual vendor result */
    unsigned long rbase;                 /* +10: sampled before FCL initialization */
    unsigned long command;              /* +14: debugger writes RV_WRITE_COMMAND */
    unsigned long confirm_before;       /* +18: debugger writes RV_EXPECTED_VECTOR */
    unsigned long write_attempts;        /* +1C: at most one per diagnostic session */
    unsigned long block_count;           /* +20: must be 38 */
    unsigned long last_block_end;        /* +24: must be 0x000FFFFF */
    unsigned long last_command;          /* +28: actual FCL command number */
    unsigned long heartbeat;             /* +2C: increments while running */
    unsigned long before[4];             /* +30: all 16 original configuration bytes */
    unsigned long after[4];              /* +40: successful SET readback only */
    unsigned long desired[4];            /* +50: before with first word changed to zero */
    unsigned long options_before[8];     /* +60: FCL GET_OPB returns 32 bytes */
    unsigned long options_after[8];      /* +80: verify options did not change */
    unsigned char device_name[16];       /* +A0: actual FCL silicon identification */
    unsigned long trap_pc;               /* +B0: FEPC sampled by RAM exception handler */
    unsigned long trap_cause;            /* +B4: FEIC sampled by RAM exception handler */
} RvMailbox;

typedef char RvLongMustBe32Bits[(sizeof(unsigned long) == 4U) ? 1 : -1];
typedef char RvMailboxMustFit[(sizeof(RvMailbox) <= 256U) ? 1 : -1];

extern volatile RvMailbox g_rv;
void RvBegin(void);
void RvPoll(void);
void RvMain(void);
unsigned long RvReadRbase(void);
int RvClockReady(void);
unsigned long RvPrepare(void);
unsigned long RvExecute(r_fcl_command_t command, unsigned long index, void *buffer);

#endif
