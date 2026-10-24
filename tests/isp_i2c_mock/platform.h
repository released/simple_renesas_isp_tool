#ifndef ISP_I2C_MOCK_PLATFORM_H
#define ISP_I2C_MOCK_PLATFORM_H
/* Native register model only. The production source is compiled unchanged. */
#pragma warning(disable:4068)
#define __far
#define __near
extern volatile unsigned char SPD0,WREL0,COI0,LREL0,TRC0,ACKE0,WTIM0,IICA0,ACKD0;
extern volatile unsigned char IICAMK0,IICA0EN,IICE0,IICAIF0,IICAPR10,IICAPR00;
extern volatile unsigned char P6,PM6,POM6,SMC0,DFC0,IICRSV0,IICCTL01;
extern volatile unsigned char IICWL0,IICWH0,SVA0,STCEN0,SPIE0;
#endif
