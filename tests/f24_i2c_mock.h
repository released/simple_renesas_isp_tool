#ifndef F24_I2C_MOCK_H
#define F24_I2C_MOCK_H
#include <stdint.h>
#define F24_CanClockIsReady MockI2cClockReady
extern volatile uint8_t IICAMK0, IICE0, IICBSY0, MSTS0, WTIM0, ACKE0;
extern volatile uint8_t IICAIF0, STT0, STD0, IICA0, ACKD0, WREL0, SPT0, PM6;
extern volatile uint8_t PRS0, SMC0, IICWL0, IICWH0;
void R_Config_IICA0_Create(void);
uint8_t F24_CanClockIsReady(void);
#define Bridge_I2cBackend_Open MockBackendOpen
#define Bridge_I2cBackend_Write MockBackendWrite
#define Bridge_I2cBackend_Read MockBackendRead
#define Bridge_I2cBackend_Poll MockBackendPoll
#define Bridge_I2cBackend_Abort MockBackendAbort
#define Bridge_I2cBackend_Irq MockBackendIrq
#endif
