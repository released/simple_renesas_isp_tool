#include "isp_device_client.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
extern "C" {
#include "device_image.h"
#if defined(ISP_XMODEM)
#include "isp_xmodem.h"
#endif
void RhRuntimeInit(unsigned long,unsigned long,unsigned long);
unsigned short RhRuntimeExchange(const unsigned char*,unsigned short,unsigned short,unsigned char*);
void RhRuntimeCompletion(unsigned char);
void RhRuntimeSendResult(unsigned char);
unsigned long RhRuntimeResets(void);
unsigned long RhRuntimeResetRequest(void);
unsigned long RhRuntimeWritten(void);
unsigned short RhRuntimeBuffered(void);
unsigned char RhRuntimeActive(void);
unsigned char RhRuntimeReady(void);
unsigned char RhRuntimeStreaming(void);
}
namespace {
using namespace firmware_upgrade;
std::vector<unsigned char> flash;
firmware_update::ImageInfo metadata;
unsigned erases=0U,pages=0U,reads=0U;
bool fail_page=false;
bool fail_final_ack=false;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::vector<unsigned char> Exchange(const std::vector<unsigned char>& tx,unsigned short advance=1U){
    unsigned char bytes[512]={};
    const auto size=RhRuntimeExchange(tx.data(),static_cast<unsigned short>(tx.size()),advance,bytes);
    return {bytes,bytes+size};
}
void Init(bool installed=true){
    flash.assign(metadata.code_flash_size,0xFFU);erases=pages=reads=0U;fail_page=fail_final_ack=false;
    RhRuntimeInit(metadata.version,metadata.build,installed?metadata.image_size:0U);
}
isp::Client Client(){
    isp::Client client;
    client.SetExchange([](const auto& tx,std::uint32_t,auto* rx,std::wstring*){*rx=Exchange(tx);return !rx->empty();});
    client.SetStreamExchange([](const auto& tx,std::uint32_t,auto* rx,std::wstring*){
        if(fail_final_ack&&tx==std::vector<unsigned char>{4U})RhRuntimeSendResult(0U);
        *rx=Exchange(tx,tx.empty()?1000U:1U);return true;
    });
    return client;
}
IspPacket Packet(unsigned short sequence,unsigned char command,const std::vector<unsigned char>& data={}){
    IspPacket packet={};packet.sequence=sequence;packet.command=command;
    packet.length=static_cast<unsigned short>(data.size());std::copy(data.begin(),data.end(),packet.data);return packet;
}
std::vector<unsigned char> Wire(const IspPacket& packet){
    unsigned char bytes[ISP_FRAME_MAX]={};const auto count=IspEncode(&packet,bytes);return {bytes,bytes+count};
}
IspPacket Request(const IspPacket& packet){
    const auto bytes=Exchange(Wire(packet));IspPacket reply={};
    Check(IspDecode(bytes.data(),static_cast<unsigned short>(bytes.size()),&reply)!=0U,"runtime framed response");return reply;
}
#if defined(ISP_XMODEM) && !defined(ISP_DEVICE_BOOT)
std::vector<unsigned char> XBlock(const std::vector<unsigned char>& data,std::size_t at){
    const auto block=static_cast<unsigned char>(at/128U+1U);
    std::vector<unsigned char> frame(133U,0x1AU);frame[0]=1U;frame[1]=block;frame[2]=static_cast<unsigned char>(~block);
    std::copy_n(data.begin()+at,(std::min)(std::size_t{128U},data.size()-at),frame.begin()+3U);
    const auto crc=IspXmodemCrc(frame.data()+3U,128U);frame[131]=static_cast<unsigned char>(crc>>8U);frame[132]=static_cast<unsigned char>(crc);
    return frame;
}
void BeginSink(const std::vector<unsigned char>& data,bool wrong_crc=false){
    std::vector<unsigned char> start(8U);IspWrite32(start.data(),static_cast<unsigned long>(data.size()));
    IspWrite32(start.data()+4U,firmware_update::Crc32(data.data(),data.size())^(wrong_crc?1U:0U));
    Check(Request(Packet(100U,ISP_CMD_STREAM_TEST,start)).status==ISP_OK&&RhRuntimeStreaming(),"actual runtime command24 stream activation");
    Check(Exchange({},1000U)==std::vector<unsigned char>{'C'},"runtime CRC-mode prompt");
}
void SinkCases(){
    for(const auto size:{389U,65536U}){
        std::vector<unsigned char> data(size);for(unsigned i=0;i<size;++i)data[i]=static_cast<unsigned char>(i^0x5AU);
        Init();BeginSink(data);
        for(std::size_t at=0;at<data.size();at+=128U){
            const auto block=XBlock(data,at);
            if(!at){auto bad=block;bad.back()^=1U;Check(Exchange(bad)==std::vector<unsigned char>{0x15U},"CRC16 NAK without consuming data");}
            Check(Exchange(block)==std::vector<unsigned char>{6U},"runtime sink ACK");
            if(!at){const auto before=RhRuntimeWritten();Check(Exchange(block)==std::vector<unsigned char>{6U}&&RhRuntimeWritten()==before,"duplicate ACK without double CRC/write");}
        }
        Check(Exchange({4U})==std::vector<unsigned char>{6U}&&!RhRuntimeActive()&&!RhRuntimeStreaming(),"full sink CRC and one EOT");
        Check(!erases&&!pages&&!RhRuntimeResets(),"sink cannot mutate Flash/reset");
        Init();BeginSink(data,true);
        for(std::size_t at=0;at<data.size();at+=128U)Exchange(XBlock(data,at));
        Check(Exchange({4U})==std::vector<unsigned char>({0x18U,0x18U})&&!RhRuntimeActive(),"full CRC mismatch cancels");
        Init();BeginSink(data);Exchange({0x18U,0x18U});Check(!RhRuntimeActive()&&!RhRuntimeStreaming(),"CAN/CAN abort reaches framed mode");
        Init();BeginSink(data);Exchange({},10001U);Check(!RhRuntimeActive()&&!RhRuntimeStreaming(),"idle timeout reaches framed mode");
        Init();BeginSink(data);auto first=XBlock(data,0U);Exchange(first);first[3]^=1U;
        const auto crc=IspXmodemCrc(first.data()+3U,128U);first[131]=static_cast<unsigned char>(crc>>8U);first[132]=static_cast<unsigned char>(crc);
        Check(Exchange(first)==std::vector<unsigned char>({0x18U,0x18U})&&!RhRuntimeActive(),"mutated duplicate cancels");
    }
}
#endif
void Run(const std::vector<unsigned char>& image){
    std::wstring error;const auto inspection=firmware_update::InspectImage(image);
    Check(firmware_update::ValidateImage(inspection.metadata.profile,image,&metadata,&error),"real BIN contract");
    Init();auto client=Client();isp::TargetInfo target;
    Check(client.Discover(&target,&error),"production client -> actual RuntimePoll INFO");
#if defined(ISP_DEVICE_BOOT)
    Check(target.mode==ISP_MODE_BOOT,"compiled Boot role");
    Check(isp::Matches(metadata,target,&error),"actual g_config matches real BIN");
    Check(client.Program(image,false,0U,{},&error),"complete client update through runtime/page/CRC");
    Check(RhRuntimeResets()==1U&&RhRuntimeResetRequest()!=0U,"one completed reset");
    Check(erases&&pages&&std::equal(image.begin(),image.end(),flash.begin()+metadata.application_start),"actual page flush bytes");
    Init(false);client=Client();RhRuntimeCompletion(0U);
    Check(client.Program(image,false,0U,{},&error),"program with delayed physical reply completion");
    Check(RhRuntimeResets()==0U,"no reset before last reply physically completes");
    RhRuntimeCompletion(1U);Exchange({});Check(RhRuntimeResets()==1U,"reset after completed final reply");
    Init(false);client=Client();fail_page=true;
    Check(!client.Program(image,false,0U,{},&error)&&RhRuntimeResets()==0U,"FCL page failure never resets");
#if defined(ISP_XMODEM)
    Init(false);client=Client();fail_final_ack=true;
    Check(!client.Program(image,false,0U,{},&error)&&!RhRuntimeResets(),"lost final ACK never resets");
#endif
#else
    Check(target.mode==
#if defined(ISP_DEVICE_APP)
        ISP_MODE_APP,"compiled App role");
    Check(target.valid&&target.ready&&RhRuntimeReady(),"App ready after observed timer tick");
#else
        ISP_MODE_TRANSFER,"compiled Transfer role");
    Check(!target.valid&&!target.ready&&!target.start&&!target.irq_abi,"Transfer no installed-image claims");
#endif
    Check(client.TransferTest(&error),"PING/ECHO/STATS and optional actual Xmodem runtime sink");
    Check(!erases&&!pages&&!RhRuntimeResets(),"diagnostics never Flash/reset");
    const auto denied=Request(Packet(500U,ISP_CMD_START,std::vector<unsigned char>(8U)));
    Check(denied.status!=ISP_OK&&!erases&&!pages,"non-Boot cannot update Flash");
#endif
    Init();
#if !defined(ISP_I2C)
    const auto wire=Wire(Packet(30U,ISP_CMD_PING,{1U,2U,3U}));
    Check(Exchange({wire.begin(),wire.begin()+5U}).empty()&&RhRuntimeBuffered()==5U,"partial UART frame retained");
    Check(!Exchange({wire.begin()+5U,wire.end()}).empty(),"split frame completes");
    Exchange({wire.begin(),wire.begin()+5U});Exchange({},201U);
    Check(!RhRuntimeBuffered(),"partial frame expires");
#endif
    const auto packet=Packet(40U,ISP_CMD_PING,{3U,4U,5U});
    const auto first=Exchange(Wire(packet));Check(!first.empty()&&Exchange(Wire(packet))==first,"exact request replay");
    auto corrupt=Wire(Packet(41U,ISP_CMD_PING));corrupt.back()^=1U;
    Check(Exchange(corrupt).empty()&&!RhRuntimeResets(),"corrupt frame has no action");
#if defined(ISP_DEVICE_APP)
    Init();RhRuntimeCompletion(0U);const auto entered=Request(Packet(50U,ISP_CMD_ENTER));
    Check(entered.status==ISP_OK&&!RhRuntimeResets(),"ENTER waits for full reply");
    RhRuntimeCompletion(1U);Exchange({});Check(RhRuntimeResets()==1U,"ENTER reset after reply");
    Init();RhRuntimeSendResult(0U);Exchange(Wire(Packet(51U,ISP_CMD_ENTER)));
    Check(!RhRuntimeResets(),"failed send cannot trigger reset");
#endif
    Check(!RhRuntimeStreaming(),"no stray stream after framed diagnostics");
#if defined(ISP_XMODEM) && !defined(ISP_DEVICE_BOOT)
    SinkCases();
#endif
    std::cout<<"PASS actual RH850 RuntimePoll role/INFO/framing/CRC/replay/Flash/completion model\n";
}
}
extern "C" {
unsigned char Device_FlashInit(void){return 1U;}
unsigned char Device_FlashErase(unsigned long address){
    Check(address>=metadata.application_start&&address+metadata.erase_block_size<=flash.size(),"erase range");++erases;
    std::fill_n(flash.begin()+address,metadata.erase_block_size,static_cast<unsigned char>(0xFFU));return 1U;
}
unsigned char Device_FlashWritePage(unsigned long address,const unsigned char* bytes){
    Check(!(address&255U)&&address>=metadata.application_start&&address+256U<=flash.size(),"page bounds");
    ++pages;if(fail_page)return 0U;std::copy_n(bytes,256U,flash.begin()+address);return 1U;
}
unsigned char Device_FlashRead(unsigned long address,unsigned char* bytes,unsigned short count){
    ++reads;if(address>=flash.size()||count>flash.size()-address)return 0U;
    std::copy_n(flash.begin()+address,count,bytes);return 1U;
}
}
int main(int argc,char** argv){try{
    Check(argc==2,"supply actual RH850 App BIN");std::ifstream stream(argv[1],std::ios::binary);Check(bool(stream),"open BIN");
    Run({std::istreambuf_iterator<char>(stream),{}});return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
