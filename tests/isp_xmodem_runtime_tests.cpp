#include "isp_device_client.h"
#include "isp_xmodem_runtime_harness.h"
#include "isp_xmodem.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace firmware_upgrade;
unsigned flash_calls=0U;
bool canceled=false, corrupt_once=false, lose_ack=false, alter_duplicate=false, bad_padding=false;
bool corrupt_payload=false, lose_eot=false;
bool cancel_on_handshake=false;
unsigned eots=0U;
void Check(bool okay,const char* what) {if(!okay)throw std::runtime_error(what);}
unsigned char Metadata(const unsigned long*) {++flash_calls;return 0U;}
unsigned char Erase(unsigned long) {++flash_calls;return 0U;}
unsigned char Write(unsigned long,const unsigned char*) {++flash_calls;return 0U;}
unsigned char Read(unsigned long,unsigned char*,unsigned short) {++flash_calls;return 0U;}
unsigned char Verify(unsigned long,unsigned long) {++flash_calls;return 0U;}
const IspDeviceOps ops={Metadata,Erase,Write,Read,Verify};
std::vector<unsigned char> Exchange(const std::vector<unsigned char>& bytes,unsigned short advance=1U) {
    unsigned char output[512]={};
    const auto n=IspRuntimeTestExchange(bytes.data(),static_cast<unsigned short>(bytes.size()),advance,output);
    return {output,output+n};
}
void SealBlock(std::vector<unsigned char>& block) {
    const auto crc=IspXmodemCrc(block.data()+3U,128U);
    block[131]=static_cast<unsigned char>(crc>>8);block[132]=static_cast<unsigned char>(crc);
}
void Bind(isp::Client& client) {
    client.SetCancelCheck([](){return canceled;});
    client.SetExchange([](const auto& request,std::uint32_t,auto* response,std::wstring*) {
        *response=Exchange(request);return !response->empty();
    });
    client.SetStreamExchange([](const auto& request,std::uint32_t,auto* response,std::wstring* why) {
        auto bytes=request;
        if(bytes.size()==133U) {
            if(corrupt_once) {bytes.back()^=1U;corrupt_once=false;}
            else if(bad_padding&&bytes[1]==4U) {bytes[8]=0U;SealBlock(bytes);}
            else if(corrupt_payload&&bytes[1]==1U) {bytes[3]^=1U;SealBlock(bytes);}
        }
        if(bytes==std::vector<unsigned char>{4U})++eots;
        *response=Exchange(bytes,bytes.empty()?1000U:1U);
        if(bytes.empty()&&cancel_on_handshake)canceled=true;
        if(lose_ack&&bytes.size()==133U&&*response==std::vector<unsigned char>{6U}) {
            const auto before=IspRuntimeTestSnapshot().written;
            lose_ack=false;
            if(alter_duplicate) {bytes[3]^=1U;SealBlock(bytes);}
            const auto replay=Exchange(bytes);
            Check(IspRuntimeTestSnapshot().written==before,"duplicate cannot accumulate CRC twice");
            Check(replay==(alter_duplicate?std::vector<unsigned char>{0x18U,0x18U}:std::vector<unsigned char>{6U}),"runtime exact duplicate content check");
            response->clear();*why=L"Injected missing block ACK";return false;
        }
        if(lose_eot&&request==std::vector<unsigned char>{4U}) {response->clear();return false;}
        return true;
    });
}
void IdleSafe() {
    const auto s=IspRuntimeTestSnapshot();
    Check(!s.active&&!s.streaming&&!s.pending&&!s.resets&&!flash_calls,"diagnostic path leaves Flash/reset untouched");
}
void Reset(const IspDeviceConfig& config,isp::Client& client) {
    IspRuntimeTestInit(&config,&ops);client.Reset();Bind(client);
    canceled=corrupt_once=lose_ack=alter_duplicate=bad_padding=corrupt_payload=lose_eot=false;
    cancel_on_handshake=false;
    flash_calls=eots=0U;
}
void Run(const std::vector<unsigned char>& image) {
    firmware_update::ImageInfo m;std::wstring error;
    Check(firmware_update::ValidateImage(firmware_update::Profile::XmodemUpgrade,image,&m,&error),"actual packaged profile input");
    IspDeviceConfig config={};config.contract=3U;config.transport=4U;config.max_write=64U;
    config.target=m.target_id;config.app_start=m.application_start;config.capacity=m.application_capacity;
    config.flash_size=m.code_flash_size;config.boot_size=m.boot_reserved_size;config.erase_size=m.erase_block_size;
    config.boot_version=m.policy.minimum_boot_version;config.boot_build=m.build;config.baud=115200UL;config.irq_abi=m.policy.irq_abi;
    for(std::size_t i=0;i<m.mcu_part.size();++i)config.part[i]=static_cast<unsigned char>(m.mcu_part[i]);
    for(std::size_t i=0;i<m.mcu_family.size();++i)config.family[i]=static_cast<unsigned char>(m.mcu_family[i]);
    isp::Client client;std::vector<unsigned char> reply;
    Reset(config,client);corrupt_once=lose_ack=true;
    Check(client.TransferTest(&error),"production TransferTest -> actual command24/runtime/CRC sink");
    Check(IspRuntimeTestSnapshot().written==389U&&eots==1U,"389 data bytes exclude Xmodem padding");IdleSafe();
    Check(client.TransferTest(&error)&&IspRuntimeTestSnapshot().written==778U,"repeated stream resets CRC, retains cumulative counters");IdleSafe();
    for(auto command:{ISP_CMD_METADATA,ISP_CMD_START,ISP_CMD_FORCE_START,ISP_CMD_WRITE,ISP_CMD_END,ISP_CMD_VERIFY}) {
        Check(!client.Request(static_cast<unsigned char>(command),{},&reply,&error),"App/Transfer refuses programming commands");IdleSafe();
    }
    for(unsigned long size:{0UL,65537UL}) {
        std::vector<unsigned char> start(8U);IspWrite32(start.data(),size);
        Check(!client.Request(ISP_CMD_STREAM_TEST,start,&reply,&error),"stream size bound before switching parser");IdleSafe();
    }
    Reset(config,client);corrupt_payload=true;
    Check(!client.TransferTest(&error)&&eots==1U,"valid block CRC16 cannot hide full diagnostic CRC32 mismatch");IdleSafe();
    Reset(config,client);bad_padding=true;
    Check(!client.TransferTest(&error)&&eots==0U,"non1A final padding cancels without EOT");IdleSafe();
    Reset(config,client);lose_ack=alter_duplicate=true;
    Check(!client.TransferTest(&error)&&eots==0U,"mutated duplicate is rejected without replay CRC accumulation");IdleSafe();
    Reset(config,client);lose_eot=true;
    Check(!client.TransferTest(&error)&&eots==1U,"lost EOT ACK remains unknown and is not repeated");IdleSafe();
    Reset(config,client);cancel_on_handshake=true;
    Check(!client.TransferTest(&error),"cancel during diagnostic streaming");
    Check(Exchange({0x18U,0x18U})==std::vector<unsigned char>({0x18U,0x18U}),"actual runtime cancel cleanup");IdleSafe();
    canceled=cancel_on_handshake=false;Check(client.TransferTest(&error),"discover/retest after cancellation");IdleSafe();
    Reset(config,client);std::vector<unsigned char> start(8U);
    IspWrite32(start.data(),389UL);IspWrite32(start.data()+4U,0UL);
    Check(client.Request(ISP_CMD_STREAM_TEST,start,&reply,&error),"start timeout session");
    Check(Exchange({},10000U)==std::vector<unsigned char>({0x18U,0x18U}),"idle timeout restores framed parser");IdleSafe();
    Check(client.TransferTest(&error),"framed discovery after timeout");IdleSafe();
    Reset(config,client);Check(client.Request(ISP_CMD_STREAM_TEST,start,&reply,&error),"start fragmented session");
    Check(Exchange({1U,1U,254U}).empty(),"partial block waits");
    Check(Exchange({},200U)==std::vector<unsigned char>({0x15U}),"partial block timeout NAK");
    Exchange({0x18U,0x18U});IdleSafe();
    Reset(config,client);std::vector<unsigned char> maximum(65536U);
    for(std::size_t at=0;at<maximum.size();++at)maximum[at]=static_cast<unsigned char>(at^(at>>8));
    IspWrite32(start.data(),static_cast<unsigned long>(maximum.size()));
    IspWrite32(start.data()+4U,firmware_update::Crc32(maximum.data(),maximum.size()));
    Check(client.Request(ISP_CMD_STREAM_TEST,start,&reply,&error)&&Exchange({},1000U)==std::vector<unsigned char>{'C'},"maximum diagnostic handshake");
    for(std::size_t at=0;at<maximum.size();at+=128U) {
        const auto number=static_cast<unsigned char>(at/128U+1U);
        std::vector<unsigned char> block(133U);block[0]=1U;block[1]=number;block[2]=static_cast<unsigned char>(~number);
        std::copy_n(maximum.begin()+at,128U,block.begin()+3U);SealBlock(block);
        Check(Exchange(block)==std::vector<unsigned char>{6U},"maximum diagnostic block including number wrap");
    }
    Check(Exchange({4U})==std::vector<unsigned char>{6U}&&IspRuntimeTestSnapshot().written==65536UL,"65536-byte runtime sink full CRC32");IdleSafe();
    std::cout<<"PASS actual Xmodem command24/transport-loop/CRC-sink role="
#if defined(ISP_DEVICE_APP)
        <<"App";
#else
        <<"Transfer";
#endif
    std::cout<<" target="<<m.target_id<<"; Flash/reset calls=0\n";
}
}
int main(int argc,char** argv) {
    try {Check(argc==2,"supply real Xmodem App BIN");std::ifstream f(argv[1],std::ios::binary);
        Check(bool(f),"open profile BIN");Run({std::istreambuf_iterator<char>(f),{}});return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
