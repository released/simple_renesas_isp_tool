#include "isp_device_client.h"
#include "isp_device.h"
#include "isp_i2c_exchange.h"
#include "isp_xmodem.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
extern "C" {
#include "isp_i2c_mailbox.h"
#include "device_image.h"
void RhIspInit(const IspDeviceConfig* config);
void RhIspProcess(const IspPacket* request,IspPacket* reply);
void RhIspStreamInit(unsigned long size);
void RhIspStreamFeed(unsigned char byte,unsigned short tick);
void RhIspStreamPoll(unsigned short tick);
unsigned char RhIspAction(void);
}
namespace {
using namespace firmware_upgrade;
IspDeviceConfig config={};
std::vector<unsigned char> flash,controls;
unsigned erases=0U,pages=0U,initializes=0U,eots=0U,ends=0U;
unsigned fail_page=0U;bool fail_erase=false,lose_end=false,cancel=false,corrupt_block=false,lose_ack=false;
unsigned short tick=0U;
void Check(bool okay,const char* text){if(!okay)throw std::runtime_error(text);}
void Seal(std::vector<unsigned char>& image){IspWrite32(image.data()+image.size()-4U,firmware_update::Crc32(image.data(),image.size()-4U));}
void Reset(const firmware_update::ImageInfo& m){
    config={};config.contract=m.profile==firmware_update::Profile::I2cUpgrade?2U:m.profile==firmware_update::Profile::XmodemUpgrade?3U:1U;
    config.transport=m.policy.transports;config.max_write=64U;config.target=m.target_id;
    config.app_start=m.application_start;config.capacity=m.application_capacity;config.flash_size=m.code_flash_size;
    config.boot_size=m.boot_reserved_size;config.erase_size=m.erase_block_size;
    config.boot_version=m.policy.minimum_boot_version;config.boot_build=m.build;config.baud=config.contract==2U?100000UL:115200UL;config.irq_abi=m.policy.irq_abi;
    for(std::size_t i=0;i<m.mcu_part.size();++i)config.part[i]=static_cast<unsigned char>(m.mcu_part[i]);
    for(std::size_t i=0;i<m.mcu_family.size();++i)config.family[i]=static_cast<unsigned char>(m.mcu_family[i]);
    flash.assign(m.code_flash_size,0xFFU);std::fill_n(flash.begin(),config.boot_size,static_cast<unsigned char>(0xA5U));
    RhIspInit(&config);erases=pages=initializes=eots=ends=0U;fail_page=0U;
    fail_erase=lose_end=cancel=corrupt_block=lose_ack=false;tick=0U;
}
void Run(const std::vector<unsigned char>& image){
    firmware_update::ImageInfo m;std::wstring error;
    auto profile=firmware_update::InspectImage(image).metadata.profile;
    Check(firmware_update::ValidateImage(profile,image,&m,&error),"actual RH850 serial BIN/entry/EBASE/512INTBP validation");
    Check(m.policy.irq_abi==0x31563852U&&m.minimum_image_size==0x1004U,"R8V1 architecture contract");
    Reset(m);isp::Client client;
    IspI2cMailbox box={};std::vector<unsigned char> pending;
    const auto direct=[&](const std::vector<unsigned char>& wire,std::uint32_t,std::vector<unsigned char>* out,std::wstring* why){
        IspStream stream={};IspPacket request={},reply={};unsigned packets=0U;
        for(auto b:wire)packets+=IspStreamFeed(&stream,b,&request);
        Check(packets==1U,"actual C framed request decode");RhIspProcess(&request,&reply);
        if(!reply.status&&config.contract==3U&&(request.command==ISP_CMD_START||request.command==ISP_CMD_FORCE_START))
            RhIspStreamInit(IspRead32(request.data));
        if(request.command==ISP_CMD_END){++ends;if(lose_end){*why=L"Injected lost END";return false;}}
        unsigned char bytes[ISP_FRAME_MAX]={};auto n=IspEncode(&reply,bytes);out->assign(bytes,bytes+n);return true;
    };
    client.SetCancelCheck([](){return cancel;});
    client.SetExchange([&](const auto& tx,std::uint32_t timeout,auto* rx,std::wstring* reason){
        if(config.contract!=2U)return direct(tx,timeout,rx,reason);
        return isp::MailboxExchange([&](const auto& wire,std::uint16_t count,std::uint16_t deadline,auto* response,std::wstring* why){
            response->clear();
            if(!wire.empty()){
                Check(!count&&IspMailboxStart(&box,0U),"I2C one request write");
                for(auto b:wire)Check(IspMailboxReceive(&box,b)!=0U,"bounded mailbox byte");
                IspMailboxStop(&box);std::vector<unsigned char> frame;unsigned char b=0U;
                while(IspMailboxGet(&box,&b))frame.push_back(b);
                if(!direct(frame,deadline,&pending,why)){IspMailboxRelease(&box);return false;}
                Check(IspMailboxPublish(&box,pending.data(),static_cast<unsigned char>(pending.size()))!=0U,"publish I2C reply");return true;
            }
            Check(count==111U&&IspMailboxStart(&box,1U),"fixed111 read");
            for(unsigned i=0;i<count;++i)response->push_back(IspMailboxNext(&box));
            IspMailboxNack(&box);IspMailboxStop(&box);return true;
        },[](){return cancel;},tx,timeout,rx,reason);
    });
    client.SetStreamExchange([](const auto& tx,std::uint32_t,auto* rx,std::wstring* why){
        controls.clear();auto bytes=tx;++tick;
        if(bytes.empty())RhIspStreamPoll(1000U);
        else{
            if(bytes==std::vector<unsigned char>{4U})++eots;
            if(corrupt_block&&bytes.size()==133U){bytes.back()^=1U;corrupt_block=false;}
            for(auto b:bytes)RhIspStreamFeed(b,tick);
        }
        *rx=controls;
        if(lose_ack&&bytes.size()==133U&&controls==std::vector<unsigned char>{6U}){
            lose_ack=false;const auto before=pages;controls.clear();
            for(auto b:bytes)RhIspStreamFeed(b,tick);
            Check(before==pages&&controls==std::vector<unsigned char>{6U},"X duplicate never repeats FCL page");
            rx->clear();*why=L"Injected lost block ACK";return false;
        }
        if(lose_end&&tx==std::vector<unsigned char>{4U}){rx->clear();return false;}
        return true;
    });
    isp::TargetInfo target;
    const bool discovered=client.Discover(&target,&error);
    const bool matched=discovered&&isp::Matches(m,target,&error);
    if(!matched)std::wcerr<<L"Discovery/match: "<<error<<L"; contract="<<unsigned(target.contract)<<L" transport="<<unsigned(target.transport)<<L"\n";
    Check(matched,"live RH850 ISP identity");
    corrupt_block=lose_ack=config.contract==3U;
    Check(client.Program(image,false,0U,{},&error),"production PC -> actual C engine/FCL page adapter");
    Check(RhIspAction()==ISP_ACTION_RESET&&std::equal(image.begin(),image.end(),flash.begin()+config.app_start),"exact programmed image and reset gate");
    Check(pages==(image.size()+255U)/256U&&erases==(image.size()+config.erase_size-1U)/config.erase_size,"FCL256-page/32KiB erase counts");
    Check(std::all_of(flash.begin(),flash.begin()+config.boot_size,[](auto b){return b==0xA5U;}),"all64KiB Boot bytes protected");
    for(std::size_t at=0x400U;at<0xC00U;at+=4U){
        auto bad=image;IspWrite32(bad.data()+at,0U);Seal(bad);
        Check(!firmware_update::ValidateImage(profile,bad,&m,&error),"each resealed INTBP slot rejected before write");
    }
    for(std::size_t at=0x200U;at<0x400U;at+=16U){auto bad=image;bad[at]=0xFFU;Seal(bad);
        Check(!firmware_update::ValidateImage(profile,bad,&m,&error),"each EBASE stub checked");}
    Check(firmware_update::ValidateImage(profile,image,&m,&error),"restore valid actual identity");
    Reset(m);IspMailboxInit(&box);fail_erase=true;
    Check(!client.Program(image,false,0U,{},&error)&&!pages&&RhIspAction()==ISP_ACTION_NONE,"erase failure has no finalization");
    Reset(m);IspMailboxInit(&box);fail_page=2U;
    Check(!client.Program(image,false,0U,{},&error)&&pages==2U&&RhIspAction()==ISP_ACTION_NONE,"partial page failure stops programming");
    Reset(m);IspMailboxInit(&box);lose_end=true;
    Check(!client.Program(image,false,0U,{},&error)&&RhIspAction()==ISP_ACTION_RESET&&(config.contract==3U?eots:ends)==1U,"lost final ACK is unknown and never repeated");
    Reset(m);IspMailboxInit(&box);auto maximum=image;maximum.resize(config.capacity,0xFFU);
    IspWrite32(maximum.data()+0x118U,config.capacity);Seal(maximum);
    Check(client.Program(maximum,false,0U,{},&error)&&pages==config.capacity/256U&&erases==30U,"max960KiB image/CRC/page aggregation");
    std::cout<<"PASS RH850 serial contract="<<unsigned(config.contract)<<" actual="<<image.size()<<" max="<<maximum.size()<<"; actual C FCL page adapter and512-vector gates\n";
}
}
extern "C" {
unsigned char Device_FlashInit(void){++initializes;return 1U;}
unsigned char Device_FlashErase(unsigned long at){
    Check(at>=config.app_start&&at+config.erase_size<=flash.size(),"FCL erase bounds");++erases;
    if(fail_erase)return 0U;
    std::fill_n(flash.begin()+at,config.erase_size,static_cast<unsigned char>(0xFFU));return 1U;
}
unsigned char Device_FlashWritePage(unsigned long at,const unsigned char* bytes){
    Check(at>=config.app_start&&!(at&255U)&&at+256U<=flash.size(),"aligned FCL page bounds");++pages;
    if(fail_page==pages)return 0U;
    std::copy_n(bytes,256U,flash.begin()+at);return 1U;
}
unsigned char Device_FlashRead(unsigned long at,unsigned char* bytes,unsigned short size){
    if(at>=flash.size()||size>flash.size()-at)return 0U;
    std::copy_n(flash.begin()+at,size,bytes);return 1U;
}
unsigned char IspBusSend(const unsigned char* bytes,unsigned short size){controls.insert(controls.end(),bytes,bytes+size);return 1U;}
}
int main(int argc,char** argv){try{Check(argc==2,"supply real RH850 ISP App BIN");std::ifstream f(argv[1],std::ios::binary);
    Check(bool(f),"open BIN");Run({std::istreambuf_iterator<char>(f),{}});return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
