#include "platform.h"
#include "isp_i2c.h"
#include <assert.h>
#include <stdio.h>
volatile unsigned char SPD0,WREL0,COI0,LREL0,TRC0,ACKE0,WTIM0,IICA0,ACKD0;
volatile unsigned char IICAMK0,IICA0EN,IICE0,IICAIF0,IICAPR10,IICAPR00;
volatile unsigned char P6,PM6,POM6,SMC0,DFC0,IICRSV0,IICCTL01;
volatile unsigned char IICWL0,IICWH0,SVA0,STCEN0,SPIE0;
static unsigned short tick;
unsigned short IspTick(void) {return tick;}
static void start(unsigned char read) {SPD0=0U;COI0=1U;TRC0=read;ACKD0=1U;IspI2cIsr();}
static void stop(void) {SPD0=1U;IspI2cIsr();SPD0=0U;}
int main(void)
{
    unsigned char at, value, reply[14];
    IspI2cInit();assert(IICA0EN&&IICE0&&!IICAMK0&&SVA0==0x40U&&SPIE0);
#if defined(ISP_SCFG_IICA_WL)
    assert(IICWL0==ISP_SCFG_IICA_WL&&IICWH0==ISP_SCFG_IICA_WH);
#if defined(ISP_MCU_G16)
    assert((IICCTL01&1U)==0U);
#else
    assert((IICCTL01&1U)==ISP_SCFG_IICA_PRS);
#endif
#elif defined(ISP_MCU_F24)
    assert(IICWL0==96U&&IICWH0==105U&&(POM6&12U)==12U);
#else
    assert(IICWL0==76U&&IICWH0==85U);
#endif
    start(0U);assert(ACKE0&&WTIM0==0U);
    for(at=0U;at<14U;++at){IICA0=at;IspI2cIsr();}
    stop();for(at=0U;at<14U;++at){assert(IspI2cGet(&value));assert(value==at);reply[at]=(unsigned char)(at^0x55U);}
    assert(!IspI2cGet(&value));assert(IspI2cSend(reply,14U));
    start(0U);assert(!ACKE0);stop();
    start(1U);assert(IICA0==0xA5U&&WTIM0);
    for(at=1U;at<111U;++at){ACKD0=1U;IspI2cIsr();assert(IICA0==(at<=14U?reply[at-1U]:255U));}
    ACKD0=0U;IspI2cIsr();stop();assert(IspI2cReplyComplete());
    start(0U);for(at=0U;at<14U;++at){IICA0=at;IspI2cIsr();}stop();
    tick=2001U;IICAMK0=1U;IspI2cPoll(tick);assert(IICAMK0==1U&&!IspI2cGet(&value));
    IspI2cStop();assert(IICAMK0&&!IICE0&&!IICAIF0);
    puts("PASS actual IICA ISR/init/STOP/NACK/mailbox/lease/mask register model; hardware NOT RUN");return 0;
}
