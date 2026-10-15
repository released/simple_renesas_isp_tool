#define BRIDGE_I2C_BACKEND_TEST
#include "../firmware/f24_board/f24_i2c_backend.c"
#include <stdio.h>

volatile uint8_t IICAMK0, IICE0, IICBSY0, MSTS0, WTIM0, ACKE0;
volatile uint8_t IICAIF0, STT0, STD0, IICA0, ACKD0, WREL0, SPT0, PM6;
static uint8_t clock_ready = 1U;
static int failures;
void R_Config_IICA0_Create(void) { IICE0 = 1U; IICAMK0 = 1U; }
uint8_t F24_CanClockIsReady(void) { return clock_ready; }
static void CheckBackend(int condition, const char *name)
{
    if (condition == 0) { printf("FAIL: F24 IICA0 %s\n", name); ++failures; }
}
static void AddressPhase(uint8_t expected)
{
    STD0 = 1U; MSTS0 = 1U; IICBSY0 = 1U; ACKD0 = 1U;
    CheckBackend(MockBackendPoll() == BRIDGE_I2C_PENDING, "start pending");
    CheckBackend(IICA0 == expected && IICAMK0 == 0U, "shift once and address before unmask");
    MockBackendIrq();
}
int TestF24I2cBackend(void)
{
    uint8_t tx[3] = {0x11U, 0x22U, 0x33U};
    uint8_t rx[3] = {0U, 0U, 0xEEU};
    failures = 0;
    clock_ready = 0U;
    CheckBackend(MockBackendOpen(0U, 0U) == BRIDGE_I2C_BUS_ERROR, "clock prerequisite");
    clock_ready = 1U;
    CheckBackend(MockBackendOpen(1U, 0U) == 3U, "invalid port");
    CheckBackend(MockBackendOpen(0U, 1U) == 3U, "unadvertised 400K");
    CheckBackend(MockBackendOpen(0U, 0U) == 0U, "open 100K");
    CheckBackend(MockBackendWrite(9U, tx, 3U, 1U) == 0U, "TX start");
    CheckBackend(IICAMK0 == 1U, "start interrupt masked");
    AddressPhase(0x12U);
    CheckBackend(IICA0 == 0x11U, "first TX byte");
    MockBackendIrq(); CheckBackend(IICA0 == 0x22U, "second TX byte");
    MockBackendIrq(); CheckBackend(IICA0 == 0x33U, "third TX byte");
    SPT0 = 0U; MockBackendIrq();
    CheckBackend(MockBackendPoll() == 0U && SPT0 == 0U, "held without STOP");
    CheckBackend(MockBackendRead(9U, rx, 2U) == 0U, "repeated-start RX");
    AddressPhase(0x13U);
    CheckBackend(WTIM0 == 0U && ACKE0 == 1U, "RX eight-clock wait");
    IICA0 = 0x44U; MockBackendIrq();
    IICA0 = 0x55U; MockBackendIrq();
    CheckBackend(rx[0] == 0x44U && rx[1] == 0x55U && rx[2] == 0xEEU &&
                 ACKE0 == 0U && WTIM0 == 1U, "last RX NACK and buffer guard");
    MockBackendIrq(); CheckBackend(SPT0 == 1U, "final STOP");
    CheckBackend(MockBackendPoll() == BRIDGE_I2C_PENDING, "wait STOP completion");
    IICBSY0 = 0U; CheckBackend(MockBackendPoll() == 0U, "complete after bus release");
    CheckBackend(MockBackendWrite(9U, tx, 1U, 0U) == 0U, "NACK test start");
    STD0 = 1U; MSTS0 = 1U; MockBackendPoll(); ACKD0 = 0U; MockBackendIrq();
    CheckBackend(MockBackendPoll() == BRIDGE_I2C_NACK, "address NACK");
    MockBackendAbort();
    CheckBackend(IICE0 == 0U && IICAMK0 == 1U && (PM6 & 0x0CU) == 0x0CU,
                 "abort releases open-drain lines");
    ACKD0 = 1U; IICBSY0 = 0U;
    MockBackendWrite(9U, tx, 1U, 0U); MockBackendPoll(); MSTS0 = 0U; MockBackendIrq();
    CheckBackend(MockBackendPoll() == BRIDGE_I2C_ARBITRATION, "lost master arbitration");
    MockBackendAbort(); MSTS0 = 1U;
    if (failures == 0) printf("F24 real IICA0 backend register/ISR mock tests passed\n");
    return failures;
}
