#include "isp_device.h"
#include "image_metadata.h"
#include "image_metadata_config.h"
#include "isp_transport_config.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
static std::vector<unsigned char> flash(IMAGE_CODE_FLASH_SIZE, 0xFFU);
static unsigned erases=0U, writes=0U;
static bool fail_erase=false, fail_write=false;
static IspDeviceConfig config={1U,0U,4U,64U,IMAGE_TARGET_ID,0U,0U,IMAGE_APPLICATION_START,IMAGE_APPLICATION_CAPACITY,
    IMAGE_CODE_FLASH_SIZE,IMAGE_RESERVED_SIZE,IMAGE_ERASE_BLOCK_SIZE,
    IMAGE_BOOT_VERSION,IMAGE_BOOT_BUILD,ISP_UART_BAUD,0UL,0UL,0UL,0U,0U,{},{},{},IMAGE_IRQ_ABI};
static unsigned char metadata(const unsigned long* words) {
    return ImageMetadataValidate(words)==IMAGE_METADATA_OK;
}
static unsigned long crc_image(const unsigned char* data,std::size_t size) {
    unsigned long crc=0xFFFFFFFFUL;
    for(std::size_t i=0;i<size;++i) {
        crc^=data[i];
        for(unsigned bit=0;bit<8U;++bit) crc=(crc>>1)^((crc&1UL)?0xEDB88320UL:0UL);
    }
    return crc^0xFFFFFFFFUL;
}
static unsigned char erase(unsigned long at) {
    CHECK(at>=config.app_start && at+1024UL<=config.app_start+config.capacity);
    ++erases; if(fail_erase) return 0U;
    std::fill(flash.begin()+at,flash.begin()+at+1024UL,static_cast<unsigned char>(0xFFU)); return 1U;
}
static unsigned char write(unsigned long at,const unsigned char* data) {
    CHECK(at>=config.app_start && at+4UL<=config.app_start+config.capacity);
    ++writes; if(fail_write) return 0U;
    std::copy(data,data+4,flash.begin()+at); return 1U;
}
static unsigned char read(unsigned long at,unsigned char* out,unsigned short size) {
    CHECK(at+size<=flash.size()); std::copy(flash.begin()+at,flash.begin()+at+size,out); return 1U;
}
static unsigned char verify(unsigned long size,unsigned long crc) {
    return IspRead32(flash.data()+config.app_start+size-4UL)==crc &&
        crc_image(flash.data()+config.app_start,size-4UL)==crc;
}
/* Fixed-profile target bindings run against the same checked native Flash
 * callbacks. They are not register/FSL/RFD emulation or hardware evidence.
 */
extern "C" unsigned char IspMetadataCheck(const unsigned long* p) { return metadata(p); }
extern "C" unsigned char IspFlashErase(unsigned long at) { return erase(at); }
extern "C" unsigned char IspFlashWrite(unsigned long at,const unsigned char* p) { return write(at,p); }
extern "C" unsigned char IspRomRead(unsigned long at,unsigned char* p,unsigned short n) { return read(at,p,n); }
extern "C" unsigned char IspImageVerify(unsigned long n,unsigned long crc) { return verify(n,crc); }
static IspDeviceOps ops={metadata,erase,write,read,verify};
static unsigned short sequence=0U;
static IspPacket packet(unsigned char command,const std::vector<unsigned char>& data={}) {
    IspPacket p={}; p.sequence=++sequence; p.command=command;
    p.length=static_cast<unsigned short>(data.size()); std::copy(data.begin(),data.end(),p.data); return p;
}
static IspPacket request(IspDevice& device,unsigned char command,const std::vector<unsigned char>& data={}) {
    const auto p=packet(command,data); IspPacket r={}; IspDeviceProcess(&device,&p,&r); return r;
}
static std::vector<unsigned char> image(unsigned long size,unsigned long version=0x10100UL) {
    std::vector<unsigned char> bytes(size,0xFFU);
    std::fill(bytes.begin()+256U,bytes.begin()+384U,static_cast<unsigned char>(0U));
    unsigned long words[32]={}; std::copy(g_image_metadata_contract,g_image_metadata_contract+32U,words);
    words[4]=version; words[5]=20261023UL; words[6]=size;
    words[27]=config.boot_version; words[29]=0UL;
    for(unsigned i=0;i<32U;++i) IspWrite32(bytes.data()+256U+4U*i,words[i]);
    IspWrite32(bytes.data()+size-4U,crc_image(bytes.data(),size-4U));
    return bytes;
}
static void stage(IspDevice& device,const std::vector<unsigned char>& bytes) {
    for(unsigned i=0;i<256U;i+=64U) {
        std::vector<unsigned char> block={static_cast<unsigned char>(i),0U};
        block.insert(block.end(),bytes.begin()+256U+i,bytes.begin()+320U+i);
        CHECK(request(device,ISP_CMD_METADATA,block).status==ISP_OK);
    }
}
static IspPacket start(IspDevice& device,const std::vector<unsigned char>& bytes,bool force=false) {
    std::vector<unsigned char> args(8U); IspWrite32(args.data(),static_cast<unsigned long>(bytes.size()));
    IspWrite32(args.data()+4U,IspRead32(bytes.data()+bytes.size()-4U));
    return request(device,force?ISP_CMD_FORCE_START:ISP_CMD_START,args);
}
static IspPacket block(IspDevice&,const std::vector<unsigned char>& bytes,unsigned offset) {
    std::vector<unsigned char> args(4U); IspWrite32(args.data(),offset);
    const auto count=std::min<std::size_t>(64U,bytes.size()-offset);
    args.insert(args.end(),bytes.begin()+offset,bytes.begin()+offset+count);
    return packet(ISP_CMD_WRITE,args);
}
static void parser_tests() {
    CHECK(IspCrc32(reinterpret_cast<const unsigned char*>("123456789"),9U)==0xCBF43926UL);
    for(unsigned length=0U;length<=ISP_PAYLOAD_MAX;++length) {
        std::vector<unsigned char> payload(length);
        for(unsigned i=0U;i<length;++i) payload[i]=static_cast<unsigned char>(i*73U);
        auto p=packet(ISP_CMD_ECHO,payload); unsigned char bytes[ISP_FRAME_MAX]={};
        const auto count=IspEncode(&p,bytes); CHECK(count==length+14U);
        IspStream stream={}; IspPacket out={};
        CHECK(!IspStreamFeed(&stream,0x49U,&out));
        for(unsigned i=0U;i<count;++i) CHECK(IspStreamFeed(&stream,bytes[i],&out)==(i+1U==count));
        CHECK(out.sequence==p.sequence && out.length==length && std::memcmp(out.data,p.data,length)==0);
        bytes[count-1U]^=0x01U; IspStreamInit(&stream);
        for(unsigned i=0U;i<count;++i) CHECK(!IspStreamFeed(&stream,bytes[i],&out));
        CHECK(stream.crc_errors==1UL);
    }
    IspStream s={}; IspPacket out={};
    for(unsigned i=0U;i<20000U;++i) { (void)IspStreamFeed(&s,0xFFU,&out); CHECK(s.count<ISP_FRAME_MAX); }
    auto p=packet(ISP_CMD_PING); p.sequence=0U; unsigned char bytes[ISP_FRAME_MAX]={}; CHECK(IspEncode(&p,bytes)==0U);
    std::puts("PASS framing: CRC vector, all 0..96 lengths, overlap, corrupt CRC, bounded noise, zero sequence");
}
static void update_tests() {
    IspDevice device={}; IspDeviceInit(&device,&config,&ops,ISP_MODE_BOOT);
    auto bytes=image(4096UL);
    CHECK(start(device,bytes).status==ISP_BAD_STATE && erases==0U && writes==0U);
    stage(device,bytes); CHECK(start(device,bytes).status==ISP_OK && erases==0U);
    auto bad=block(device,bytes,64U); IspPacket reply={}; IspDeviceProcess(&device,&bad,&reply);
    CHECK(reply.status==ISP_BAD_SEQUENCE && writes==0U);
    for(unsigned offset=0U;offset<bytes.size();offset+=64U) {
        auto p=block(device,bytes,offset); IspDeviceProcess(&device,&p,&reply); CHECK(reply.status==ISP_OK);
        const auto before=writes; IspDeviceProcess(&device,&p,&reply); CHECK(reply.status==ISP_OK && writes==before);
        p.data[4]^=1U; IspDeviceProcess(&device,&p,&reply); CHECK(reply.status==ISP_BAD_SEQUENCE && writes==before);
    }
    CHECK(erases==4U && writes==1024U && !device.valid);
    CHECK(request(device,ISP_CMD_END).status==ISP_OK && device.action==ISP_ACTION_RESET);
    CHECK(std::equal(bytes.begin(),bytes.end(),flash.begin()+config.app_start));
    CHECK(flash[config.app_start+config.capacity]==0xFFU && flash[config.app_start-1U]==0xFFU);
    std::puts("PASS update: pre-START gates, ordered writes, duplicate replay, same-sequence conflict, END/CRC, protected bounds");
}
static void policy_fault_tests() {
    auto bytes=image(1024UL,0x10000UL); IspDevice d={};
    IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT); IspDeviceInstalled(&d,0x10100UL,1UL,1024UL,1U);
    stage(d,bytes); const auto before=erases;
    CHECK(start(d,bytes).status==ISP_BAD_VERSION && erases==before);
    CHECK(start(d,bytes,true).status==ISP_OK); CHECK(request(d,ISP_CMD_ABORT).status==ISP_OK);
    stage(d,bytes); CHECK(start(d,bytes).status==ISP_BAD_VERSION);
    IspDeviceInstalled(&d,0x10100UL,1UL,1024UL,0U); CHECK(start(d,bytes).status==ISP_OK);
    fail_erase=true; auto p=block(d,bytes,0U); IspPacket r={}; IspDeviceProcess(&d,&p,&r);
    CHECK(r.status==ISP_FLASH_ERROR && !d.active && !d.valid); fail_erase=false;
    IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT); stage(d,bytes); CHECK(start(d,bytes).status==ISP_OK);
    fail_write=true; p=block(d,bytes,0U); IspDeviceProcess(&d,&p,&r);
    CHECK(r.status==ISP_FLASH_ERROR && !d.active); fail_write=false;
    IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT); bytes[256]^=1U; stage(d,bytes);
    CHECK(start(d,bytes,true).status==ISP_BAD_METADATA);
    IspDeviceInit(&d,&config,&ops,ISP_MODE_APP);
    CHECK(request(d,ISP_CMD_START,std::vector<unsigned char>(8U)).status==ISP_BAD_STATE);
    CHECK(request(d,ISP_CMD_ENTER).status==ISP_OK && d.action==ISP_ACTION_ENTER);
    IspDeviceInit(&d,&config,&ops,ISP_MODE_TRANSFER);
    CHECK(request(d,ISP_CMD_ENTER).status==ISP_BAD_COMMAND && d.action==ISP_ACTION_NONE);
    CHECK(request(d,ISP_CMD_WRITE,std::vector<unsigned char>(8U)).status==ISP_BAD_COMMAND);
    CHECK(request(d,ISP_CMD_INFO).length==88U);
    std::puts("PASS policy/faults: valid-App floor, request-local FORCE, disabled floor, erase/write failures, App/Transfer isolation");
}
static void capacity_tests() {
    for(const unsigned long size : {65532UL,65536UL,65540UL,config.capacity}) {
        if(size>config.capacity)continue;
        auto bytes=image(size); IspDevice d={}; IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT);
        stage(d,bytes); CHECK(start(d,bytes).status==ISP_OK);
        IspPacket r={};
        for(unsigned offset=0U;offset<size;offset+=64U) {
            auto p=block(d,bytes,offset); IspDeviceProcess(&d,&p,&r); CHECK(r.status==ISP_OK);
        }
        CHECK(request(d,ISP_CMD_END).status==ISP_OK);
        CHECK(std::equal(bytes.begin(),bytes.end(),flash.begin()+config.app_start));
        CHECK(flash[config.app_start+config.capacity]==0xFFU);
    }
    auto bytes=image(config.capacity+4UL); IspDevice d={}; IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT);
    stage(d,bytes); const auto before=writes;
    CHECK(start(d,bytes).status==ISP_BAD_METADATA && writes==before);
    bytes=image(1024UL); IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT);
    stage(d,bytes); CHECK(start(d,bytes).status==ISP_OK);
    auto p=block(d,bytes,0U); p.length=8U; IspPacket r={}; IspDeviceProcess(&d,&p,&r);
    CHECK(r.status==ISP_BAD_SIZE && writes==before);
    std::puts("PASS capacity: supported 64-KiB boundaries, maximum image, overflow, short nonfinal block, protected final 1 KiB");
}
static void metadata_fault_tests() {
    for(unsigned word=0U;word<64U;++word) {
        auto bytes=image(1024UL); IspDevice d={}; IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT);
        const auto at=256U+word*4U;
        unsigned long value=IspRead32(bytes.data()+at)^1UL;
        if(word==4U) value=0x01000000UL;
        if(word==5U) value=0xFFFFFFFFUL;
        if(word==6U) value=512UL;
        if(word==27U) value=config.boot_version+1UL;
        if(word==29U) value=2UL;
        IspWrite32(bytes.data()+at,value); stage(d,bytes); const auto before=writes;
        CHECK(start(d,bytes,true).status==ISP_BAD_METADATA && writes==before);
    }
    auto bytes=image(1024UL); IspDevice d={}; IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT);
    stage(d,bytes); CHECK(start(d,bytes).status==ISP_OK); IspPacket r={};
    for(unsigned offset=0U;offset<256U;offset+=64U) {
        auto p=block(d,bytes,offset); IspDeviceProcess(&d,&p,&r); CHECK(r.status==ISP_OK);
    }
    auto p=block(d,bytes,256U); p.data[4U]^=1U; const auto before=writes;
    IspDeviceProcess(&d,&p,&r); CHECK(r.status==ISP_BAD_METADATA && writes==before);
    p=block(d,bytes,256U); IspDeviceProcess(&d,&p,&r); CHECK(r.status==ISP_OK);
    for(unsigned offset=320U;offset<bytes.size();offset+=64U) {
        p=block(d,bytes,offset); IspDeviceProcess(&d,&p,&r); CHECK(r.status==ISP_OK);
    }
    flash[config.app_start+512U]^=1U;
    CHECK(request(d,ISP_CMD_END).status==ISP_CRC_ERROR && d.action==ISP_ACTION_NONE);
    std::puts("PASS metadata/faults: all64 staged words, FORCE identity gates, staged/write mismatch, Flash CRC corruption");
}
static void compiled_bin_test(const char* path) {
    std::ifstream stream(path,std::ios::binary); CHECK(stream.good());
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    CHECK(bytes.size()>=516U && bytes.size()<=config.capacity && !(bytes.size()&3U));
    IspDevice d={}; IspDeviceInit(&d,&config,&ops,ISP_MODE_BOOT);
    stage(d,bytes); CHECK(start(d,bytes).status==ISP_OK); IspPacket r={};
    for(unsigned offset=0U;offset<bytes.size();offset+=64U) {
        auto p=block(d,bytes,offset); IspDeviceProcess(&d,&p,&r); CHECK(r.status==ISP_OK);
    }
    CHECK(request(d,ISP_CMD_END).status==ISP_OK && d.action==ISP_ACTION_RESET);
    CHECK(std::equal(bytes.begin(),bytes.end(),flash.begin()+config.app_start));
    std::puts("PASS actual CS+ packaged App BIN: staged metadata, ordered writes, exact Flash bytes, END CRC");
}
int main(int argc,char** argv) {
    parser_tests(); update_tests(); policy_fault_tests(); capacity_tests(); metadata_fault_tests();
    if(argc==2) compiled_bin_test(argv[1]);
    return 0;
}
