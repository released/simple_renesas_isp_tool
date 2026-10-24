#include "isp_device_client.h"
#include "isp_device.h"
#include "isp_xmodem.h"
#if defined(ISP_TEST_RUNTIME)
#include "isp_xmodem_runtime_harness.h"
#endif
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
extern "C" {
#include "image_metadata.h"
}
namespace {
using namespace firmware_upgrade;
IspDeviceConfig config={};
std::vector<unsigned char> flash, controls;
unsigned writes=0U, erases=0U, finishes=0U, eots=0U;
unsigned short tick=0U;
bool cancel_requested=false, lose_block_ack=false, corrupt_block=false;
bool lose_eot_ack=false, corrupt_body=false, corrupt_trailer=false;
bool fail_erase=false;unsigned fail_write_at=0U;
bool fail_control_tx=false;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
unsigned char Metadata(const unsigned long* words){return ImageMetadataValidate(words)==IMAGE_METADATA_OK;}
unsigned char Erase(unsigned long at){
    Check(at>=config.app_start&&at+config.erase_size<=config.app_start+config.capacity,"erase protected bounds");
    ++erases;if(fail_erase)return 0U;
    std::fill(flash.begin()+at,flash.begin()+at+config.erase_size,static_cast<unsigned char>(0xFF));return 1U;
}
unsigned char Write(unsigned long at,const unsigned char* bytes){
    Check(at>=config.app_start&&at+4UL<=config.app_start+config.capacity,"write protected bounds");
    ++writes;if(fail_write_at==writes)return 0U;
    std::copy_n(bytes,4U,flash.begin()+at);return 1U;
}
unsigned char Read(unsigned long at,unsigned char* bytes,unsigned short size){
    if(at>=flash.size()||size>flash.size()-at)return 0U;
    std::copy_n(flash.begin()+at,size,bytes);return 1U;
}
unsigned char Verify(unsigned long size,unsigned long crc){
    if(size<516UL||size>config.capacity||IspRead32(flash.data()+config.app_start+size-4U)!=crc||
       firmware_update::Crc32(flash.data()+config.app_start,size-4U)!=crc)return 0U;
    unsigned long words[64]={};
    for(unsigned i=0U;i<64U;++i)words[i]=IspRead32(flash.data()+config.app_start+256U+i*4U);
    if(words[6]!=size||!Metadata(words))return 0U;
    for(unsigned at=0U;at<256U;at+=4U){
        if(at==4U&&config.irq_abi!=0x31563246UL)continue;
        const auto p=flash.data()+config.app_start+at;
        const auto target=static_cast<unsigned long>(p[1])|(static_cast<unsigned long>(p[2])<<8)|(static_cast<unsigned long>(p[3])<<16);
        if(p[0]!=0xECU||target<config.app_start+512U||target>=config.app_start+size-4U)return 0U;
    }
    return 1U;
}
IspDeviceOps ops={Metadata,Erase,Write,Read,Verify};
void Configure(const firmware_update::ImageInfo& m){
    config={};config.contract=3U;config.transport=4U;config.max_write=64U;config.target=m.target_id;
    config.app_start=m.application_start;config.capacity=m.application_capacity;config.flash_size=m.code_flash_size;
    config.boot_size=m.boot_reserved_size;config.erase_size=m.erase_block_size;
    config.boot_version=m.policy.minimum_boot_version;config.boot_build=m.build;config.baud=115200UL;config.irq_abi=m.policy.irq_abi;
    for(std::size_t i=0U;i<m.mcu_part.size();++i)config.part[i]=static_cast<unsigned char>(m.mcu_part[i]);
    for(std::size_t i=0U;i<m.mcu_family.size();++i)config.family[i]=static_cast<unsigned char>(m.mcu_family[i]);
    flash.assign(config.flash_size,0xFFU);
#if defined(ISP_TEST_RUNTIME)
    IspRuntimeTestInit(&config,&ops);
#else
    IspDeviceInit(&g_isp_device,&config,&ops,ISP_MODE_BOOT);
#endif
    IspStreamInit(&g_isp_input.stream);writes=erases=finishes=eots=0U;tick=0U;
    cancel_requested=lose_block_ack=corrupt_block=lose_eot_ack=corrupt_body=corrupt_trailer=false;
    fail_erase=false;fail_write_at=0U;fail_control_tx=false;
}
void Reseal(std::vector<unsigned char>& image){
    IspWrite32(image.data()+image.size()-4U,firmware_update::Crc32(image.data(),image.size()-4U));
}
void Stage(isp::Client& client,const std::vector<unsigned char>& image){
    std::vector<unsigned char> reply;std::wstring error;
    for(unsigned at=0U;at<256U;at+=64U){
        std::vector<unsigned char> part{static_cast<unsigned char>(at),0U};
        part.insert(part.end(),image.begin()+256U+at,image.begin()+320U+at);
        Check(client.Request(ISP_CMD_METADATA,part,&reply,&error),"stage256 via actual framed client");
    }
}
void Run(const std::vector<unsigned char>& image){
    firmware_update::ImageInfo m;std::wstring error;
    Check(firmware_update::ValidateImage(firmware_update::Profile::XmodemUpgrade,image,&m,&error),"actual packaged Xmodem App BIN");
    Configure(m);isp::Client client;
    client.SetCancelCheck([](){return cancel_requested;});
    client.SetExchange([](const std::vector<unsigned char>& wire,std::uint32_t,
        std::vector<unsigned char>* response,std::wstring*){
#if defined(ISP_TEST_RUNTIME)
        unsigned char bytes[512]={};
        const auto count=IspRuntimeTestExchange(wire.data(),static_cast<unsigned short>(wire.size()),1U,bytes);
        response->assign(bytes,bytes+count);return count!=0U;
#else
        IspStreamInit(&g_isp_input.stream);unsigned packets=0U;
        for(auto byte:wire)packets+=IspStreamFeed(&g_isp_input.stream,byte,&g_isp_request);
        Check(packets==1U,"actual C framed control decode");
        IspDeviceProcess(&g_isp_device,&g_isp_request,&g_isp_reply);
        unsigned char bytes[ISP_FRAME_MAX]={};auto count=IspEncode(&g_isp_reply,bytes);
        response->assign(bytes,bytes+count);
        if(!g_isp_reply.status&&(g_isp_request.command==ISP_CMD_START||g_isp_request.command==ISP_CMD_FORCE_START))
            IspXmodemInit(&g_isp_input.x,g_isp_device.image_size,0U);
        return true;
#endif
    });
    client.SetStreamExchange([](const std::vector<unsigned char>& tx,std::uint32_t,
        std::vector<unsigned char>* response,std::wstring* why){
        controls.clear();auto bytes=tx;
        if(bytes.empty()) {
#if defined(ISP_TEST_RUNTIME)
            unsigned char output[512]={};const auto n=IspRuntimeTestExchange(nullptr,0U,1000U,output);
            controls.assign(output,output+n);
#else
            IspXmodemPoll(&g_isp_input.x,1000U,nullptr);
#endif
        }
        else{
            ++tick;
            if(bytes.size()==1U&&bytes[0]==4U){
                ++eots;
                if(corrupt_body)flash[config.app_start+512U]^=1U;
                if(corrupt_trailer)flash[config.app_start+g_isp_device.image_size-1U]^=1U;
            }
            if(corrupt_block&&bytes.size()==133U){bytes.back()^=1U;corrupt_block=false;}
#if defined(ISP_TEST_RUNTIME)
            if(fail_control_tx)IspRuntimeTestSendResult(0U);
            unsigned char output[512]={};const auto n=IspRuntimeTestExchange(bytes.data(),static_cast<unsigned short>(bytes.size()),1U,output);
            controls.assign(output,output+n);
#else
            for(auto byte:bytes)IspXmodemFeed(&g_isp_input.x,byte,tick,nullptr);
#endif
        }
        response->assign(controls.begin(),controls.end());
        if(lose_block_ack&&tx.size()==133U&&*response==std::vector<unsigned char>{6U}){
            lose_block_ack=false;const auto before=writes;controls.clear();
#if defined(ISP_TEST_RUNTIME)
            unsigned char output[512]={};const auto n=IspRuntimeTestExchange(tx.data(),static_cast<unsigned short>(tx.size()),1U,output);
            controls.assign(output,output+n);
#else
            for(auto byte:tx)IspXmodemFeed(&g_isp_input.x,byte,tick,nullptr);
#endif
            Check(writes==before&&controls==std::vector<unsigned char>{6U},"exact replay never repeats Flash");
            response->clear();*why=L"Injected lost block ACK.";return false;
        }
        if(lose_eot_ack&&tx==std::vector<unsigned char>{4U}){response->clear();*why=L"Injected lost EOT ACK.";return false;}
        return true;
    });
    isp::TargetInfo target;std::vector<unsigned char> reply;
    Check(client.Discover(&target,&error)&&target.contract==3U&&isp::Matches(m,target,&error),"live Xmodem Boot discovery/profile");
    Check(IspDeviceWrite(&g_isp_device,0U,image.data(),128U)==ISP_BAD_STATE&&IspDeviceFinish(&g_isp_device)==ISP_BAD_STATE,"internal data path requires START");
    Check(!client.Request(ISP_CMD_WRITE,std::vector<unsigned char>(8U),&reply,&error)&&!client.Request(ISP_CMD_END,{},&reply,&error)&&writes==0U,"no framed WRITE/END on Xmodem Boot");
    lose_block_ack=true;corrupt_block=true;
    Check(client.Program(image,false,0U,{},&error),"production client -> actual X receiver -> actual Boot engine");
    Check(finishes==1U&&eots==1U&&g_isp_device.action==ISP_ACTION_RESET&&
        std::equal(image.begin(),image.end(),flash.begin()+config.app_start),"exact image and one-shot finalization");
    for(unsigned word=0U;word<64U;++word){
        if(word==4U||word==5U||word==6U||word==27U||word==29U)continue;
        Configure(m);auto bad=image;bad[256U+word*4U]^=1U;Stage(client,bad);
        std::vector<unsigned char> start(8U);IspWrite32(start.data(),static_cast<unsigned long>(image.size()));
        IspWrite32(start.data()+4U,IspRead32(image.data()+image.size()-4U));
        Check(!client.Request(ISP_CMD_FORCE_START,start,&reply,&error)&&!g_isp_device.active&&writes==0U&&erases==0U,"FORCE keeps all fixed metadata gates");
    }
    Configure(m);IspDeviceInstalled(&g_isp_device,m.version+1U,m.build,m.image_size,1U);
    Check(!client.Program(image,false,0U,{},&error)&&writes==0U,"valid-App downgrade floor before Flash");
    Check(!client.Program(image,true,m.version+2U,{},&error)&&writes==0U,"stale force approval");
    Check(client.Program(image,true,m.version+1U,{},&error),"transaction-only forced downgrade");
    Configure(m);IspDeviceInstalled(&g_isp_device,m.version,m.build,m.image_size,1U);
    Check(client.Program(image,false,0U,{},&error),"enabled policy permits same version");
    Configure(m);IspDeviceInstalled(&g_isp_device,m.version-1U,m.build,m.image_size,1U);
    Check(client.Program(image,false,0U,{},&error),"enabled policy permits newer version");
    Configure(m);IspDeviceInstalled(&g_isp_device,m.version+1U,m.build,m.image_size,0U);
    Check(client.Program(image,false,0U,{},&error),"disabled policy permits older version");
    Configure(m);fail_erase=true;
    Check(!client.Program(image,false,0U,{},&error)&&erases==1U&&writes==0U&&!g_isp_device.active&&finishes==0U,"erase failure cancels without data ACK/reset");
    Configure(m);fail_write_at=6U;
    Check(!client.Program(image,false,0U,{},&error)&&erases==1U&&writes==6U&&!g_isp_device.active&&finishes==0U,"partial block write failure cancels without data ACK/reset");
    Configure(m);corrupt_body=true;
    Check(!client.Program(image,false,0U,{},&error)&&finishes==1U&&g_isp_device.action==ISP_ACTION_NONE,"full image CRC failure prevents reset");
    Configure(m);corrupt_trailer=true;
    Check(!client.Program(image,false,0U,{},&error)&&finishes==1U&&g_isp_device.action==ISP_ACTION_NONE,"stored CRC trailer failure prevents reset");
    Configure(m);lose_eot_ack=true;
    Check(!client.Program(image,false,0U,{},&error)&&eots==1U&&finishes==1U&&g_isp_device.action==ISP_ACTION_RESET,"unknown EOT result is never retried");
    Configure(m);
    Check(!client.Program(image,false,0U,[](unsigned value,const wchar_t*){if(value>=2U)cancel_requested=true;},&error)&&writes==32U,"cancel after one128-byte block");
    controls.clear();
#if defined(ISP_TEST_RUNTIME)
    {const unsigned char cancel[]={0x18U,0x18U};unsigned char output[512]={};IspRuntimeTestExchange(cancel,2U,1U,output);}
#else
    IspXmodemFeed(&g_isp_input.x,0x18U,tick,nullptr);IspXmodemFeed(&g_isp_input.x,0x18U,tick,nullptr);
#endif
    Check(!g_isp_device.active&&!g_isp_device.staged&&g_isp_device.action==ISP_ACTION_NONE,"cancel cleanup without finalization");
    Configure(m);auto maximum=image;maximum.resize(config.capacity,0xFFU);
    IspWrite32(maximum.data()+280U,config.capacity);Reseal(maximum);
    Check(client.Program(maximum,false,0U,{},&error)&&
        std::equal(maximum.begin(),maximum.end(),flash.begin()+config.app_start),"maximum image / block wrap / protected final1KiB");
#if defined(ISP_TEST_RUNTIME)
    Check(IspRuntimeTestSnapshot().resets==1U,"successful final ACK causes exactly one reset");
    Configure(m);
    Check(!client.Program(image,false,0U,[](unsigned p,const wchar_t*){if(p>=2U)fail_control_tx=true;},&error),"local block ACK TX failure stops update");
    Check(!g_isp_device.active&&!IspRuntimeTestSnapshot().pending&&!IspRuntimeTestSnapshot().resets,"block ACK failure clears stream/reset");
    Configure(m);
    Check(!client.Program(image,false,0U,[](unsigned p,const wchar_t*){if(p>=92U)fail_control_tx=true;},&error),"local final ACK TX failure is not success");
    Check(!IspRuntimeTestSnapshot().pending&&!IspRuntimeTestSnapshot().resets&&!g_isp_device.active,"final ACK failure never resets");
    IspRuntimeTestSendResult(1U);
    {unsigned char output[512]={};IspRuntimeTestExchange(nullptr,0U,1U,output);}
    Check(!IspRuntimeTestSnapshot().resets,"later loop cannot consume a stale reset");
    Configure(m);Stage(client,image);
    std::vector<unsigned char> start(8U);IspWrite32(start.data(),static_cast<unsigned long>(image.size()));IspWrite32(start.data()+4U,IspRead32(image.data()+image.size()-4U));
    Check(client.Request(ISP_CMD_START,start,&reply,&error),"start before handshake failure");
    IspRuntimeTestSendResult(0U);
    {unsigned char output[512]={};IspRuntimeTestExchange(nullptr,0U,1000U,output);}
    Check(!g_isp_device.active&&!IspRuntimeTestSnapshot().pending&&!IspRuntimeTestSnapshot().resets,"failed C handshake cancels without reset");
#endif
    std::cout<<"PASS Xmodem actual BIN/client/C receiver/fixed Boot context; actual="<<image.size()<<", maximum="<<maximum.size()<<" bytes\n";
}
}
extern "C" {
unsigned char IspMetadataCheck(const unsigned long* words){return Metadata(words);}
unsigned char IspFlashErase(unsigned long at){return Erase(at);}
unsigned char IspFlashWrite(unsigned long at,const unsigned char* data){return Write(at,data);}
unsigned char IspRomRead(unsigned long at,unsigned char* data,unsigned short size){return Read(at,data,size);}
unsigned char IspImageVerify(unsigned long size,unsigned long crc){
#if defined(ISP_TEST_RUNTIME)
    ++finishes;
#endif
    return Verify(size,crc);
}
#if !defined(ISP_TEST_RUNTIME)
unsigned char IspXmodemWrite(unsigned long at,const unsigned char* data,unsigned short size){return IspDeviceWrite(&g_isp_device,at,data,size)==ISP_OK;}
unsigned char IspXmodemFinish(){++finishes;return IspDeviceFinish(&g_isp_device)==ISP_OK;}
void IspXmodemAbort(){g_isp_device.active=0U;g_isp_device.staged=0U;g_isp_device.action=ISP_ACTION_NONE;}
void IspXmodemSend(unsigned char byte){controls.push_back(byte);}
#endif
}
int main(int argc,char** argv){
    try{Check(argc==2,"supply actual App BIN");std::ifstream input(argv[1],std::ios::binary);Check(bool(input),"open actual BIN");
        Run({std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()});return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
