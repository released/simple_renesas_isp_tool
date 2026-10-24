#ifndef ISP_I2C_H
#define ISP_I2C_H
void IspI2cInit(void);
void IspI2cStop(void);
void IspI2cPoll(unsigned short now);
unsigned char IspI2cGet(unsigned char *value);
unsigned char IspI2cSend(const unsigned char *data, unsigned short size);
void IspI2cRelease(void);
unsigned char IspI2cReplyComplete(void);
void IspI2cIsr(void);
#endif
