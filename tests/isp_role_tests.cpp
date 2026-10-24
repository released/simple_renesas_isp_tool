#include "isp_device.h"
#include "image_metadata_config.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL role line %d\n",__LINE__); std::exit(1); } } while(0)
static unsigned flash_calls=0U;
static unsigned char metadata(const unsigned long*) { return 0U; }
static unsigned char erase(unsigned long) { ++flash_calls; return 0U; }
static unsigned char write(unsigned long,const unsigned char*) { ++flash_calls; return 0U; }
static unsigned char read(unsigned long,unsigned char*,unsigned short) { return 0U; }
static unsigned char verify(unsigned long,unsigned long) { return 0U; }
int main() {
    IspDeviceConfig c={}; c.app_start=IMAGE_APPLICATION_START; c.capacity=IMAGE_APPLICATION_CAPACITY;
    c.flash_size=IMAGE_CODE_FLASH_SIZE; c.boot_size=IMAGE_RESERVED_SIZE; c.erase_size=IMAGE_ERASE_BLOCK_SIZE;
    c.baud=115200UL; c.contract=1U; c.max_write=64U; c.transport=4U;
    IspDeviceOps ops={metadata,erase,write,read,verify};
    IspDevice d={};
    /* Even an accidental wrong internal mode cannot turn an App or Transfer
     * binary into a Flash writer; the compiled role is authoritative.
     */
    IspDeviceInit(&d,&c,&ops,99U);
    IspPacket q={},r={}; q.sequence=1U; q.command=ISP_CMD_INFO;
    IspDeviceProcess(&d,&q,&r); CHECK(r.status==ISP_OK && r.length==88U);
#if defined(ISP_DEVICE_BOOT)
    CHECK(r.data[1]==ISP_MODE_BOOT && r.data[54]==0x0FU);
#elif defined(ISP_DEVICE_APP)
    CHECK(r.data[1]==ISP_MODE_APP && r.data[54]==0x3CU);
#else
    CHECK(r.data[1]==ISP_MODE_TRANSFER && r.data[54]==0x10U);
    CHECK(IspRead32(r.data+8U)==0UL && IspRead32(r.data+28U)==0UL && IspRead32(r.data+84U)==0UL);
#endif
    const unsigned char commands[]={ISP_CMD_START,ISP_CMD_FORCE_START,ISP_CMD_WRITE,ISP_CMD_END};
    for(const auto cmd:commands) {
        q.sequence++; q.command=cmd; q.length=(cmd==ISP_CMD_END)?0U:8U;
        IspDeviceProcess(&d,&q,&r); CHECK(r.status!=ISP_OK && flash_calls==0U);
    }
    q.sequence++; q.command=ISP_CMD_ENTER; q.length=0U; IspDeviceProcess(&d,&q,&r);
#if defined(ISP_DEVICE_APP)
    CHECK(r.status==ISP_OK && d.action==ISP_ACTION_ENTER);
#elif defined(ISP_DEVICE_BOOT)
    CHECK(r.status==ISP_OK && d.action==ISP_ACTION_NONE);
#else
    CHECK(r.status==ISP_BAD_COMMAND && d.action==ISP_ACTION_NONE);
#endif
    q.sequence++; q.command=ISP_CMD_PING; IspDeviceProcess(&d,&q,&r);
#if defined(ISP_DEVICE_BOOT)
    CHECK(r.status==ISP_BAD_COMMAND);
#else
    CHECK(r.status==ISP_OK);
#endif
    std::puts("PASS compiled role: live INFO/capabilities, no cross-role Flash access, ENTER and diagnostic ownership");
}
