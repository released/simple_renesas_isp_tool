#include "isp_device_client.h"
#include "isp_i2c_exchange.h"
#include "isp_device.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
extern "C" {
#include "image_metadata.h"
#include "isp_i2c_mailbox.h"
}
namespace {
using namespace firmware_upgrade;
std::vector<std::uint8_t> flash;
IspDeviceConfig config={};IspDevice device={};
unsigned writes=0U,erases=0U;
bool custom_bus=false;
std::uint8_t custom_address=0x20U;
void Check(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);}
void Reseal(std::vector<std::uint8_t>* image) {
    auto crc=firmware_update::Crc32(image->data(),image->size()-4U);
    for(std::size_t i=image->size()-4U;i<image->size();++i) {
        (*image)[i]=static_cast<std::uint8_t>(crc);crc>>=8U;
    }
}
unsigned char Metadata(const unsigned long* words) {
    if(!custom_bus)return ImageMetadataValidate(words)==IMAGE_METADATA_OK;
    // Model a customer-rebuilt rate/address profile while keeping the real
    // parser for every other word. No product profile or test BIN is changed.
    unsigned long normalized[64];std::copy(words,words+64U,normalized);
    if(config.contract==2U) {
        if(words[12]!=config.baud||words[13]!=custom_address)return 0U;
        normalized[12]=g_image_metadata_contract[12];normalized[13]=g_image_metadata_contract[13];
    } else {
        if(words[28]!=config.baud)return 0U;
        normalized[28]=g_image_metadata_contract[28];
    }
    return ImageMetadataValidate(normalized)==IMAGE_METADATA_OK;
}
unsigned char Erase(unsigned long address) {
    Check(address>=config.app_start&&address+config.erase_size<=config.app_start+config.capacity,"erase bounds");
    std::fill(flash.begin()+address,flash.begin()+address+config.erase_size,std::uint8_t{0xFFU});++erases;return 1U;
}
unsigned char Write(unsigned long address,const unsigned char* bytes) {
    Check(address>=config.app_start&&address+4U<=config.app_start+config.capacity,"write bounds");
    std::copy(bytes,bytes+4U,flash.begin()+address);++writes;return 1U;
}
unsigned char Read(unsigned long address,unsigned char* bytes,unsigned short count) {
    if(address>=flash.size()||count>flash.size()-address)return 0U;
    std::copy(flash.begin()+address,flash.begin()+address+count,bytes);return 1U;
}
unsigned char Verify(unsigned long size,unsigned long crc) {
    return size>=516U&&size<=config.capacity&&
        IspRead32(flash.data()+config.app_start+size-4U)==crc&&
        firmware_update::Crc32(flash.data()+config.app_start,size-4U)==crc;
}
IspDeviceOps ops={Metadata,Erase,Write,Read,Verify};
void Configure(const firmware_update::ImageInfo& m) {
    config={};config.contract=m.profile==firmware_update::Profile::I2cUpgrade?2U:1U;config.transport=m.policy.transports;config.max_write=64U;config.target=m.target_id;
    config.app_start=m.application_start;config.capacity=m.application_capacity;config.flash_size=m.code_flash_size;
    config.boot_size=m.boot_reserved_size;config.erase_size=m.erase_block_size;
    config.boot_version=m.policy.minimum_boot_version;config.boot_build=m.build;
    config.baud=config.contract==2U?m.nominal_bitrate_mask:m.policy.device_uart_baud;config.irq_abi=m.policy.irq_abi;
    for(std::size_t i=0U;i<m.mcu_part.size();++i)config.part[i]=static_cast<unsigned char>(m.mcu_part[i]);
    for(std::size_t i=0U;i<m.mcu_family.size();++i)config.family[i]=static_cast<unsigned char>(m.mcu_family[i]);
    flash.assign(config.flash_size,0xFFU);IspDeviceInit(&device,&config,&ops,ISP_MODE_BOOT);
}
void Run(const std::vector<std::uint8_t>& image) {
    firmware_update::ImageInfo m;std::wstring error;
    const auto profile=firmware_update::InspectImage(image).metadata.profile;
    Check(firmware_update::ValidateImage(profile,image,&m,&error),"actual ISP BIN validation");
    Check(!firmware_update::ValidateImage(firmware_update::Profile::CanUpgrade,image,&m,&error),"UART BIN cannot use CAN workflow");
    Configure(m);isp::Client client;bool corrupt=false,wrongseq=false,cancel=false,lose_end=false;
    unsigned end_commands=0U,write_commands=0U;
    client.SetCancelCheck([&](){return cancel;});
    isp::Exchange direct=[&](const std::vector<std::uint8_t>& wire,std::uint32_t,
        std::vector<std::uint8_t>* response,std::wstring* exchange_error) {
        IspStream stream={};IspPacket request={},reply={};unsigned packets=0U;
        for(auto b:wire)packets+=IspStreamFeed(&stream,b,&request);
        Check(packets==1U,"production client framing decoded by real C");
        if(request.command==ISP_CMD_END)++end_commands;
        if(request.command==ISP_CMD_WRITE)++write_commands;
        IspDeviceProcess(&device,&request,&reply);
        if(lose_end&&request.command==ISP_CMD_END) {
            *exchange_error=L"Injected lost END reply.";return false;
        }
        if(wrongseq)++reply.sequence;
        unsigned char bytes[ISP_FRAME_MAX]={};auto count=IspEncode(&reply,bytes);
        response->assign(bytes,bytes+count);if(corrupt)response->back()^=1U;
        return true;
    };
    IspI2cMailbox mailbox={};std::vector<std::uint8_t> pending;
    client.SetExchange([&](const std::vector<std::uint8_t>& tx,std::uint32_t timeout,
        std::vector<std::uint8_t>* rx,std::wstring* reason) {
        if(config.contract!=2U)return direct(tx,timeout,rx,reason);
        return isp::MailboxExchange([&](const std::vector<std::uint8_t>& wire,std::uint16_t count,
            std::uint16_t deadline,std::vector<std::uint8_t>* response,std::wstring* why) {
            response->clear();
            if(!wire.empty()) {
                Check(count==0U&&IspMailboxStart(&mailbox,0U),"one STOP-delimited mailbox request");
                for(auto byte:wire)Check(IspMailboxReceive(&mailbox,byte)!=0U,"mailbox request byte");
                IspMailboxStop(&mailbox);std::vector<std::uint8_t> frame;unsigned char b=0;
                while(IspMailboxGet(&mailbox,&b))frame.push_back(b);
                if(!direct(frame,deadline,&pending,why)){IspMailboxRelease(&mailbox);return false;}
                Check(IspMailboxPublish(&mailbox,pending.data(),static_cast<unsigned char>(pending.size()))!=0U,"publish immutable reply");return true;
            }
            Check(count==111U&&IspMailboxStart(&mailbox,1U),"mailbox read window");
            for(unsigned i=0;i<count;++i)response->push_back(IspMailboxNext(&mailbox));
            IspMailboxNack(&mailbox);IspMailboxStop(&mailbox);return true;
        },[&](){return cancel;},tx,timeout,rx,reason);
    });
    isp::TargetInfo target;
    Check(client.Discover(&target,&error)&&target.mode==ISP_MODE_BOOT&&!target.valid,"empty Boot discovery");
    Check(isp::Matches(m,target,&error),"live/image match");
    auto other=target;other.irq_abi^=1U;Check(!isp::Matches(m,other,&error),"IRQ mismatch rejected");
    unsigned previous=0U;
    Check(client.Program(image,false,0U,[&](unsigned value,const wchar_t*){
        Check(value>=previous&&value<=96U,"progress ordered, no premature100");previous=value;
    },&error),"real C Boot update from production PC client");
    Check(device.action==ISP_ACTION_RESET&&std::equal(image.begin(),image.end(),flash.begin()+config.app_start),"exact Flash bytes and END reset request");
    IspDeviceInit(&device,&config,&ops,ISP_MODE_APP);
    IspDeviceInstalled(&device,m.version,m.build,m.image_size,0U);device.ready=1U;
    Check(client.Discover(&target,&error)&&target.ready&&target.valid&&target.mode==ISP_MODE_APP,"App ready distinct from Boot");
    std::vector<std::uint8_t> metadata;
    Check(client.ReadMetadata(&metadata,&error)&&isp::ValidateInstalledMetadata(metadata,target,&error),"all256 live metadata bytes validated");
    metadata[255]^=1U;Check(!isp::ValidateInstalledMetadata(metadata,target,&error),"reserved metadata readback corruption rejected");
    Check(client.TransferTest(&error),"actual App PING/ECHO/STATS");
    IspDeviceInit(&device,&config,&ops,ISP_MODE_BOOT);
    IspDeviceInstalled(&device,m.version+1U,m.build,m.image_size,1U);
    const auto before=writes;
    Check(!client.Program(image,false,0U,{},&error)&&writes==before,"downgrade blocked before erase/write");
    Check(!client.Program(image,true,m.version+2U,{},&error)&&writes==before,"stale force approval blocked");
    Check(client.Program(image,true,m.version+1U,{},&error),"request-local forced downgrade");
    IspDeviceInit(&device,&config,&ops,ISP_MODE_TRANSFER);
    Check(client.Discover(&target,&error)&&target.mode==ISP_MODE_TRANSFER&&!target.capacity&&
        !target.boot&&target.baud==config.baud,"Transfer exposes no Boot/App geometry");
    Check(client.TransferTest(&error),"actual standalone Transfer diagnostic");
    corrupt=true;Check(!client.Discover(&target,&error),"bad reply CRC rejected");corrupt=false;
    wrongseq=true;Check(!client.Discover(&target,&error),"wrong sequence rejected");wrongseq=false;
    cancel=true;Check(!client.Discover(&target,&error),"cancel before device exchange");
    cancel=false;
    Configure(m);const auto staged_before=writes;
    Check(!client.Program(image,false,0U,[&](unsigned value,const wchar_t*){
        if(value==1U)cancel=true;
    },&error)&&writes==staged_before&&!device.active,"cancel after metadata does not START/write");
    cancel=false;Configure(m);const auto writing_before=write_commands;
    Check(!client.Program(image,false,0U,[&](unsigned value,const wchar_t*){
        if(value>=2U)cancel=true;
    },&error)&&write_commands==writing_before+1U,"cancel after first WRITE sends no further WRITE");
    cancel=false;Configure(m);lose_end=true;const auto ending_before=end_commands;
    Check(!client.Program(image,false,0U,{},&error)&&device.action==ISP_ACTION_RESET&&
        end_commands==ending_before+1U,"unknown END result is not automatically repeated");
    lose_end=false;
    auto bad=image;bad[0x130U]=1U;Reseal(&bad);
    Check(!firmware_update::ValidateImage(profile,bad,&m,&error),"ISP link field mutation rejected");
    bad=image;bad[0]=0xFFU;Reseal(&bad);
    Check(!firmware_update::ValidateImage(profile,bad,&m,&error),"resealed invalid App jump opcode rejected");
    bad=image;bad[1]=0U;bad[2]=0U;bad[3]=0U;Reseal(&bad);
    Check(!firmware_update::ValidateImage(profile,bad,&m,&error),"resealed App jump outside image rejected");
    auto custom=image;
    const auto put=[&](std::size_t offset,std::uint32_t value) {
        for(unsigned i=0U;i<4U;++i){custom[offset+i]=static_cast<std::uint8_t>(value);value>>=8U;}
    };
    if(profile==firmware_update::Profile::I2cUpgrade){put(0x130U,400000U);put(0x134U,0x42U);custom_address=0x42U;}
    else put(0x170U,123456U);
    Reseal(&custom);Check(firmware_update::ValidateImage(profile,custom,&m,&error),"custom rate/address BIN validation");
    Configure(m);custom_bus=true;
    isp::BusConfiguration settings{0U,config.baud,custom_address};
    client.SetBusConfiguration(config.contract,settings);
    Check(client.Discover(&target,&error)&&isp::Matches(m,target,&error),"custom selection/live/BIN contract match");
    auto wrong=settings;wrong.baud=115200U;client.SetBusConfiguration(config.contract,wrong);
    if(wrong.baud!=config.baud)Check(!client.Discover(&target,&error),"selected/live rate mismatch blocked");
    client.SetBusConfiguration(config.contract,settings);
    if(config.contract==2U) {
        auto wrong_address=settings;++wrong_address.address;client.SetBusConfiguration(config.contract,wrong_address);
        const auto unchanged=erases;
        Check(!client.Program(custom,false,0U,{},&error)&&erases==unchanged,"selected I2C address mismatch blocks before erase");
        client.SetBusConfiguration(config.contract,settings);
    }
    Check(client.Program(custom,false,0U,{},&error),"custom rate/address full update through real C engine");
    IspDeviceInit(&device,&config,&ops,ISP_MODE_APP);IspDeviceInstalled(&device,m.version,m.build,m.image_size,0U);device.ready=1U;
    Check(client.Discover(&target,&error)&&client.ReadMetadata(&metadata,&error)&&
        isp::ValidateInstalledMetadata(metadata,target,&error),"custom installed metadata readback");
    std::cout<<"PASS actual BIN + production ISP client + real C Boot/App/Transfer; writes="<<writes<<", erases="<<erases<<"\n";
}
}
int main(int argc,char** argv) {
    try {
        Check(argc==2,"supply actual packaged App BIN");std::ifstream in(argv[1],std::ios::binary);
        Check(bool(in),"open BIN");Run({std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()});
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
