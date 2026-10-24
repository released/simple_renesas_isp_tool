#include "isp_device_client.h"
#include "isp_device.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
extern "C" {
#include "image_metadata.h"
void RlRuntimeInit(const IspDeviceOps*,unsigned long,unsigned long,unsigned long);
unsigned short RlRuntimeExchange(const unsigned char*,unsigned short,unsigned short,unsigned char*);
void RlRuntimeSendResult(unsigned char);
void RlRuntimeCompletion(unsigned char);
unsigned long RlRuntimeResets(void);
unsigned char RlRuntimeActive(void);
unsigned short RlRuntimeBuffered(void);
}
namespace {
using namespace firmware_upgrade;
std::vector<unsigned char> flash;
firmware_update::ImageInfo metadata;
unsigned writes=0U,erases=0U;
bool erase_failure=false,write_failure=false,verify_failure=false;
bool fail_final_ack=false;
void Check(bool okay,const char* label){if(!okay)throw std::runtime_error(label);}
unsigned char Metadata(const unsigned long* words){return ImageMetadataValidate(words)==IMAGE_METADATA_OK;}
unsigned char Erase(unsigned long at){
    Check(at>=metadata.application_start&&at+metadata.erase_block_size<=metadata.application_start+metadata.application_capacity,"erase boundaries");
    ++erases;if(erase_failure)return 0U;
    std::fill(flash.begin()+at,flash.begin()+at+metadata.erase_block_size,static_cast<unsigned char>(0xFFU));return 1U;
}
unsigned char Write(unsigned long at,const unsigned char* bytes){
    Check(at>=metadata.application_start&&at+4U<=metadata.application_start+metadata.application_capacity,"write boundaries");
    ++writes;if(write_failure)return 0U;
    std::copy_n(bytes,4U,flash.begin()+at);return 1U;
}
unsigned char Read(unsigned long at,unsigned char* bytes,unsigned short size){
    if(at>=flash.size()||size>flash.size()-at)return 0U;
    std::copy_n(flash.begin()+at,size,bytes);return 1U;
}
unsigned char Verify(unsigned long size,unsigned long crc){
    return !verify_failure&&size>=516U&&size<=metadata.application_capacity&&
        IspRead32(flash.data()+metadata.application_start+size-4U)==crc&&
        firmware_update::Crc32(flash.data()+metadata.application_start,size-4U)==crc;
}
const IspDeviceOps ops={Metadata,Erase,Write,Read,Verify};
std::vector<unsigned char> Exchange(const std::vector<unsigned char>& bytes,unsigned short advance=1U){
    unsigned char output[512]={};const auto n=RlRuntimeExchange(bytes.data(),static_cast<unsigned short>(bytes.size()),advance,output);
    return {output,output+n};
}
void Reset(isp::Client& client){
    flash.assign(metadata.code_flash_size,0xFFU);writes=erases=0U;
    erase_failure=write_failure=verify_failure=fail_final_ack=false;
    RlRuntimeInit(&ops,metadata.version,metadata.build,metadata.image_size);client.Reset();
    client.SetExchange([](const auto& bytes,std::uint32_t,auto* response,std::wstring*){
        *response=Exchange(bytes);return !response->empty();
    });
    client.SetStreamExchange([](const auto& bytes,std::uint32_t,auto* response,std::wstring*){
        if(fail_final_ack&&bytes==std::vector<unsigned char>{4U})RlRuntimeSendResult(0U);
        *response=Exchange(bytes,bytes.empty()?1000U:1U);return !response->empty();
    });
}
void Run(const std::vector<unsigned char>& image){
    const auto profile=firmware_update::InspectImage(image).metadata.profile;std::wstring error;
    Check(firmware_update::ValidateImage(profile,image,&metadata,&error),"real App image");
    isp::Client client;isp::TargetInfo target;std::vector<unsigned char> reply;
    Reset(client);Check(client.Discover(&target,&error),"actual runtime INFO");
    Check(target.target==metadata.target_id,"actual project config");
    auto broken=std::vector<unsigned char>(14U,0U);Check(Exchange(broken).empty(),"malformed frame rejected");
    Check(client.Discover(&target,&error),"discover after malformed request");
#if !defined(ISP_I2C)
    Check(Exchange({0x49U,0x53U,1U}).empty()&&RlRuntimeBuffered()!=0U,"partial frame buffered");
    Exchange({},200U);Check(RlRuntimeBuffered()==0U,"partial-frame timeout");
#endif
#if defined(ISP_DEVICE_BOOT)
    Check(!(target.capabilities&ISP_CAP_DIAGNOSTICS),"compact Boot does not advertise diagnostics");
    const auto programmed=client.Program(image,false,0U,{},&error);
    if(!programmed)std::wcerr<<error<<L'\n';
    Check(programmed,"production client through actual Boot runtime");
    Check(RlRuntimeResets()==1U&&std::equal(image.begin(),image.end(),flash.begin()+metadata.application_start),"one reset after exact image");
    Exchange({});Check(RlRuntimeResets()==1U,"no stale repeated reset");
    Reset(client);erase_failure=true;Check(!client.Program(image,false,0U,{},&error)&&!RlRuntimeResets(),"erase failure no reset");
    Reset(client);write_failure=true;Check(!client.Program(image,false,0U,{},&error)&&!RlRuntimeResets(),"write failure no reset");
    Reset(client);verify_failure=true;Check(!client.Program(image,false,0U,{},&error)&&!RlRuntimeResets(),"CRC failure no reset");
#if defined(ISP_XMODEM)
    Reset(client);fail_final_ack=true;
    Check(!client.Program(image,false,0U,{},&error)&&!RlRuntimeResets()&&!RlRuntimeActive(),"failed final ACK clears reset");
    RlRuntimeSendResult(1U);Exchange({});Check(!RlRuntimeResets(),"failed ACK has no late reset");
#endif
#else
    Check(client.TransferTest(&error)&&!writes&&!erases&&!RlRuntimeResets(),"actual App/Transfer diagnostics never Flash");
#if !defined(ISP_I2C)
    Check(client.Discover(&target,&error)&&(target.capabilities&ISP_CAP_DIAGNOSTICS),"optional diagnostic capability");
    isp::UartDiagnostics diagnostics;
    Check(client.ReadDiagnostics(target,&diagnostics,&error)&&diagnostics.rx_errors==2U&&diagnostics.rx_drops==3U&&diagnostics.tx_timeouts==4U,"actual device diagnostic callback and PC parser");
    auto invalid=std::vector<unsigned char>(24U,0U);Check(!isp::ParseDiagnostics(invalid,&diagnostics,&error),"unknown diagnostic schema rejected");
#endif
    Check(!client.Request(ISP_CMD_START,{},&reply,&error)&&!writes&&!erases,"non-Boot refuses programming");
#if defined(ISP_DEVICE_APP)
    Reset(client);RlRuntimeSendResult(0U);
    Check(!client.Request(ISP_CMD_ENTER,{},&reply,&error)&&!RlRuntimeResets(),"ENTER TX failure no reset");
    Reset(client);RlRuntimeCompletion(0U);
    Check(client.Request(ISP_CMD_ENTER,{},&reply,&error),"ENTER accepted");
#if defined(ISP_I2C)
    Check(!RlRuntimeResets(),"I2C waits full ready read/NACK/STOP");
    RlRuntimeCompletion(1U);Exchange({});Check(RlRuntimeResets()==1U,"complete I2C reply releases reset");
#else
    Check(RlRuntimeResets()==1U,"synchronous UART reply completes before reset");
#endif
#endif
#endif
    std::cout<<"PASS actual RL78 runtime target="<<metadata.target_id<<" profile="<<static_cast<unsigned>(profile)<<"\n";
}
}
int main(int argc,char** argv){
    try{Check(argc==2,"supply actual App BIN");std::ifstream input(argv[1],std::ios::binary);Check(bool(input),"open BIN");
        Run({std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()});return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
