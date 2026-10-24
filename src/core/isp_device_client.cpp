#include "isp_device_client.h"
#include <algorithm>
#include <cstring>

namespace firmware_upgrade::isp {
namespace {
bool Fail(std::wstring* error,const std::wstring& text) { if(error)*error=text; return false; }
std::uint16_t Le16(const std::uint8_t* p) {return static_cast<std::uint16_t>(p[0]|(p[1]<<8));}
std::uint32_t Le32(const std::uint8_t* p) {return image_policy::Read32(p);}
void Put32(std::vector<std::uint8_t>& p,std::uint32_t n) {
    for(unsigned i=0;i<4U;++i) {p.push_back(static_cast<std::uint8_t>(n));n>>=8;}
}
bool Text(const std::uint8_t* p,std::size_t n,std::wstring* text) {
    bool ended=false; text->clear();
    for(std::size_t i=0;i<n;++i) {
        if(!p[i])ended=true;
        else if(ended||p[i]<32U||p[i]>126U)return false;
        else text->push_back(p[i]);
    }
    return !text->empty();
}
const wchar_t* Status(unsigned s) {
    const wchar_t* names[]={L"OK",L"BAD_COMMAND",L"BAD_SIZE",L"BAD_STATE",L"BAD_SEQUENCE",
        L"BAD_METADATA",L"BAD_VERSION",L"FLASH_ERROR",L"CRC_ERROR"};
    return s<9U?names[s]:L"UNKNOWN_STATUS";
}
}
void Client::SetExchange(Exchange exchange) {exchange_=std::move(exchange);Reset();}
void Client::SetCancelCheck(std::function<bool()> canceled) {canceled_=std::move(canceled);}
void Client::SetStreamExchange(Exchange exchange) {stream_exchange_=std::move(exchange);}
void Client::SetBusConfiguration(std::uint8_t contract,const BusConfiguration& configuration) {
    contract_=contract;configuration_=configuration;
}
void Client::Reset() {sequence_=0U;}
bool Client::Request(std::uint8_t command,const std::vector<std::uint8_t>& payload,
    std::vector<std::uint8_t>* reply,std::wstring* error,std::uint32_t timeout) {
    if(reply)reply->clear();
    if(!exchange_||!reply||payload.size()>ISP_PAYLOAD_MAX||!timeout||timeout>30000U)
        return Fail(error,L"Invalid ISP exchange, payload or timeout.");
    if(canceled_&&canceled_())return Fail(error,L"ISP operation canceled.");
    IspPacket request={};
    if(++sequence_==0U)++sequence_;
    request.sequence=sequence_;request.command=command;request.length=static_cast<unsigned short>(payload.size());
    std::copy(payload.begin(),payload.end(),request.data);
    unsigned char encoded[ISP_FRAME_MAX]={};const auto count=IspEncode(&request,encoded);
    std::vector<std::uint8_t> received;
    // Only Host Link's identical EXEC can replay. A lost device reply is not
    // permission to repeat ENTER/START/WRITE/END with a fresh device sequence.
    if(!exchange_({encoded,encoded+count},timeout,&received,error))return false;
    IspStream stream={};IspPacket response={};unsigned complete=0U;
    for(const auto byte:received)complete+=IspStreamFeed(&stream,byte,&response);
    if(complete!=1U||stream.count||received.size()!=response.length+14U||stream.crc_errors||stream.framing_errors||
        !response.response||response.sequence!=request.sequence||response.command!=command||response.status>ISP_CRC_ERROR)
        return Fail(error,L"ISP response framing/CRC/sequence/command mismatch. Device command was not repeated.");
    if(response.status)return Fail(error,L"Device ISP command "+std::to_wstring(command)+L" returned "+Status(response.status)+L".");
    reply->assign(response.data,response.data+response.length);
    return true;
}
bool ParseInfo(const std::vector<std::uint8_t>& bytes,TargetInfo* target,std::wstring* error) {
    if(!target||bytes.size()!=88U)return Fail(error,L"ISP INFO must contain exactly 88 bytes.");
    const auto* p=bytes.data();TargetInfo t;
    t.contract=p[0];t.transport=p[2];t.mode=p[1];t.target=Le16(p+4);t.valid=p[6]!=0;t.active=p[7]!=0;
    t.start=Le32(p+8);t.capacity=Le32(p+12);t.flash=Le32(p+16);t.boot=Le32(p+20);
    t.erase=Le32(p+24);t.boot_version=Le32(p+28);t.boot_build=Le32(p+32);t.baud=Le32(p+36);
    t.version=Le32(p+40);t.build=Le32(p+44);t.size=Le32(p+48);t.version_control=p[52]!=0;
    t.ready=p[53]!=0;t.capabilities=p[54];t.irq_abi=Le32(p+84);
    if(t.contract<1U||t.contract>3U||t.transport!=(t.contract==2U?IP_I2C:IP_UART)||p[3]!=64U||t.mode>ISP_MODE_TRANSFER||!t.target||
        p[6]>1U||p[7]>1U||p[52]>1U||p[53]>1U||p[55]||
        !(t.contract==2U?ValidI2cRate(t.baud):ValidUartBaud(t.baud))||
        (t.capabilities&~ISP_CAP_DIAGNOSTICS)!=(t.mode==ISP_MODE_BOOT?0x0FU:t.mode==ISP_MODE_APP?0x3CU:0x10U)||
        ((t.capabilities&ISP_CAP_DIAGNOSTICS)&&(t.mode==ISP_MODE_BOOT||t.contract==2U))||
        !Text(p+56,16U,&t.part)||!Text(p+72,12U,&t.family))return Fail(error,L"Invalid ISP INFO identity/transport/capabilities.");
    if(t.mode==ISP_MODE_TRANSFER) {
        if(t.start||t.capacity||t.flash||t.boot||t.erase||t.boot_version||t.boot_build||t.version||
            t.build||t.size||t.valid||t.active||t.ready||t.version_control||t.irq_abi)
            return Fail(error,L"Standalone Transfer must not claim Boot/App metadata.");
    } else if(!t.start||!t.capacity||!t.erase||(t.erase&(t.erase-1U))||t.erase<64U||
        t.start<t.boot||t.start>=t.flash||t.capacity>t.flash-t.start||t.start%t.erase||t.capacity%t.erase||
        !t.irq_abi||t.irq_abi==0xFFFFFFFFU||t.boot_version>0xFFFFFFU||t.boot_build==0xFFFFFFFFU||
        (t.valid&&(t.size<516U||t.size>t.capacity||(t.size&3U)||t.version>0xFFFFFFU||t.build==0xFFFFFFFFU))||
        (!t.valid&&(t.version||t.build||t.size||t.version_control||t.ready))||
        (t.mode==ISP_MODE_APP&&(!t.valid||t.active)))return Fail(error,L"Invalid ISP live Flash/App contract.");
    *target=std::move(t);return true;
}
bool ParseDiagnostics(const std::vector<std::uint8_t>& bytes,UartDiagnostics* out,std::wstring* error) {
    if(!out||bytes.size()!=ISP_DIAGNOSTIC_BYTES||bytes[0]!=1U||bytes[1]!=1U||bytes[2]!=0x0FU||bytes[3])
        return Fail(error,L"Unsupported or malformed device UART diagnostics snapshot.");
    UartDiagnostics d;d.rx_errors=Le32(bytes.data()+4);d.rx_drops=Le32(bytes.data()+8);
    d.tx_timeouts=Le32(bytes.data()+12);d.rx_peak=Le32(bytes.data()+16);d.rx_pending=Le32(bytes.data()+20);
    if(d.rx_pending>d.rx_peak||d.rx_peak>65535U)return Fail(error,L"Device UART queue diagnostics are inconsistent.");
    *out=d;return true;
}
bool Client::ReadDiagnostics(const TargetInfo& target,UartDiagnostics* out,std::wstring* error) {
    if(!(target.capabilities&ISP_CAP_DIAGNOSTICS)||target.mode==ISP_MODE_BOOT||target.contract==2U)
        return Fail(error,L"Device does not advertise UART diagnostics; no diagnostic command was sent.");
    std::vector<std::uint8_t> bytes;
    return Request(ISP_CMD_DIAGNOSTICS,{},&bytes,error)&&ParseDiagnostics(bytes,out,error);
}
bool Client::Discover(TargetInfo* target,std::wstring* error) {
    std::vector<std::uint8_t> bytes;
    bool ok=Request(ISP_CMD_INFO,{},&bytes,error);
    // A new PC session may reuse the last cached device sequence. INFO is
    // read-only; resolve that one conflict without retrying any write command.
    if(!ok&&error&&error->find(L"BAD_SEQUENCE")!=std::wstring::npos)
        ok=Request(ISP_CMD_INFO,{},&bytes,error);
    if(!ok||!ParseInfo(bytes,target,error))return false;
    if(contract_) {
        if(!ValidBus(contract_,configuration_)||target->contract!=contract_||target->baud!=configuration_.baud)
            return Fail(error,L"Selected device bus rate/protocol differs from live INFO. Check Boot/App settings and Discover again.");
        target->address=configuration_.address;
    }
    return true;
}
bool Matches(const firmware_update::ImageInfo& m,const TargetInfo& t,std::wstring* error) {
    const auto profile=t.contract==2U?firmware_update::Profile::I2cUpgrade:t.contract==3U?firmware_update::Profile::XmodemUpgrade:firmware_update::Profile::UartUpgrade;
    if(m.profile!=profile||t.mode==ISP_MODE_TRANSFER||
        m.target_id!=t.target||m.application_start!=t.start||m.application_capacity!=t.capacity||
        m.code_flash_size!=t.flash||m.boot_reserved_size!=t.boot||m.erase_block_size!=t.erase||
        m.policy.transports!=t.transport||m.policy.irq_abi!=t.irq_abi||m.policy.minimum_boot_version>t.boot_version||
        m.policy.device_uart_baud!=(t.contract==2U?0U:t.baud)||
        (t.contract==2U&&(m.nominal_bitrate_mask!=t.baud||m.data_bitrate_mask!=t.address))||
        m.mcu_part!=t.part||m.mcu_family!=t.family)
        return Fail(error,L"BIN interface/protocol, identity, Flash layout, IRQ ABI, minimum Boot or rate differs from live INFO.");
    return true;
}
bool Client::ReadMetadata(std::vector<std::uint8_t>* bytes,std::wstring* error) {
    if(!bytes)return Fail(error,L"Null installed metadata output.");bytes->clear();
    for(unsigned at=0U;at<256U;at+=64U) {
        std::vector<std::uint8_t> page;
        if(!Request(ISP_CMD_READ_METADATA,{static_cast<std::uint8_t>(at),0U,64U},&page,error))return false;
        if(page.size()!=64U)return Fail(error,L"Installed metadata page length mismatch.");
        bytes->insert(bytes->end(),page.begin(),page.end());
    }
    return true;
}
bool ValidateInstalledMetadata(const std::vector<std::uint8_t>& bytes,const TargetInfo& t,std::wstring* error) {
    if(bytes.size()!=256U||!t.valid)return Fail(error,L"No valid installed App metadata.");
    const auto* p=bytes.data();image_policy::Policy policy;std::wstring part,family;
    const auto protocol=t.contract==2U?0x49U:t.contract==3U?0x58U:0x55U;
    if(Le32(p)!=0x00505349U||Le32(p+4)!=0x00038006U||Le32(p+8)!=(0x01010000U|protocol)||
        Le32(p+12)!=t.target||Le32(p+16)!=t.version||Le32(p+20)!=t.build||Le32(p+24)!=t.size||
        Le32(p+28)!=t.start||Le32(p+32)!=t.capacity||Le32(p+36)!=t.flash||Le32(p+40)!=t.boot||
        Le32(p+44)!=t.erase||Le32(p+48)!=(t.contract==2U?t.baud:0U)||Le32(p+52)!=(t.contract==2U?t.address:0U)||Le32(p+56)||Le32(p+60)||
        !Text(p+64,16U,&part)||!Text(p+80,12U,&family)||part!=t.part||family!=t.family||
        Le32(p+92)!=(t.irq_abi==0x31563852U?0x1004U:516U)||
        !image_policy::Parse(p,&policy)||policy.version!=IP_DEVICE_VERSION||policy.role!=IP_ROLE_DEVICE_APP||
        policy.transports!=t.transport||policy.device_uart_baud!=(t.contract==2U?0U:t.baud)||policy.irq_abi!=t.irq_abi||
        policy.minimum_boot_version>t.boot_version||policy.version_control!=static_cast<unsigned>(t.version_control)||
        !std::all_of(bytes.begin()+128,bytes.end(),[](std::uint8_t b){return b==0xFFU;}))
        return Fail(error,L"Installed App metadata readback differs from live INFO or UART image contract.");
    return true;
}
bool Client::TransferTest(std::wstring* error) {
    TargetInfo t;if(!Discover(&t,error))return false;
    if((t.capabilities&ISP_CAP_TRANSFER)==0U)return Fail(error,L"Selected device does not advertise diagnostic transfer.");
    for(const auto command:{ISP_CMD_PING,ISP_CMD_ECHO}) {
        std::vector<std::uint8_t> data(command==ISP_CMD_ECHO?96U:3U),reply;
        for(std::size_t i=0;i<data.size();++i)data[i]=static_cast<std::uint8_t>(i^0xA5U);
        if(!Request(static_cast<std::uint8_t>(command),data,&reply,error))return false;
        if(reply!=data)return Fail(error,L"UART Transfer echo content mismatch.");
    }
    std::vector<std::uint8_t> reply;
    if(t.contract==3U) {
        std::vector<std::uint8_t> data(389U),start;
        for(std::size_t i=0;i<data.size();++i)data[i]=static_cast<std::uint8_t>(i^0x5AU);
        Put32(start,static_cast<std::uint32_t>(data.size()));Put32(start,firmware_update::Crc32(data.data(),data.size()));
        if(!Request(ISP_CMD_STREAM_TEST,start,&reply,error)||!reply.empty()||!Stream(data,{},error))return false;
    }
    return Request(ISP_CMD_STATS,{},&reply,error)&&(reply.size()==16U||Fail(error,L"UART diagnostic counters must contain 16 bytes."));
}
bool Client::Program(const std::vector<std::uint8_t>& image,bool force,std::uint32_t approved_version,
    std::function<void(unsigned,const wchar_t*)> progress,std::wstring* error) {
    firmware_update::ImageInfo m;TargetInfo t;std::vector<std::uint8_t> reply;
    if(!Discover(&t,error))return false;
    const auto profile=t.contract==2U?firmware_update::Profile::I2cUpgrade:t.contract==3U?firmware_update::Profile::XmodemUpgrade:firmware_update::Profile::UartUpgrade;
    if(!firmware_update::ValidateImage(profile,image,&m,error)||!Matches(m,t,error))return false;
    if(t.mode!=ISP_MODE_BOOT||t.active)return Fail(error,L"Discover App then Enter Boot before Start Device Update.");
    if(force&&(!t.valid||!t.version_control||m.version>=t.version||t.version!=approved_version))
        return Fail(error,L"Force approval no longer matches the live installed version/policy.");
    if(!force&&t.valid&&t.version_control&&m.version<t.version)return Fail(error,L"Installed App blocks an older version.");
    if(progress)progress(0U,L"1/5 Metadata");
    for(unsigned at=0U;at<256U;at+=64U) {
        std::vector<std::uint8_t> page{static_cast<std::uint8_t>(at),0U};
        page.insert(page.end(),image.begin()+256U+at,image.begin()+320U+at);
        if(!Request(ISP_CMD_METADATA,page,&reply,error)||!reply.empty())return Fail(error,error&& !error->empty()?*error:L"Unexpected metadata ACK payload.");
    }
    std::vector<std::uint8_t> start;Put32(start,m.image_size);Put32(start,m.stored_crc32);
    if(progress)progress(1U,L"2/5 Start");
    if(!Request(force?ISP_CMD_FORCE_START:ISP_CMD_START,start,&reply,error)||!reply.empty())return false;
    if(t.contract==3U) {
        if(!Stream(image,progress,error))return false;
        if(progress)progress(96U,L"5/5 Confirm App");return true;
    }
    for(std::size_t at=0U;at<image.size();) {
        const auto count=(std::min)(std::size_t{64U},image.size()-at);
        std::vector<std::uint8_t> block;Put32(block,static_cast<std::uint32_t>(at));
        block.insert(block.end(),image.begin()+at,image.begin()+at+count);
        if(!Request(ISP_CMD_WRITE,block,&reply,error,3000U)||!reply.empty())return false;
        at+=count;
        if(progress)progress(2U+static_cast<unsigned>(at*88U/image.size()),L"3/5 Write");
    }
    if(progress)progress(92U,L"4/5 END CRC");
    if(!Request(ISP_CMD_END,{},&reply,error,5000U)||!reply.empty())return false;
    if(progress)progress(96U,L"5/5 Confirm App");
    return true;
}
bool Client::Stream(const std::vector<std::uint8_t>& bytes,
    std::function<void(unsigned,const wchar_t*)> progress,std::wstring* error) {
    if(!stream_exchange_)return Fail(error,L"XMODEM raw UART exchange is not configured.");
    std::vector<std::uint8_t> reply;
    if(!stream_exchange_({},2000U,&reply,error)||reply!=std::vector<std::uint8_t>{'C'})
        return Fail(error,L"XMODEM receiver CRC handshake was not confirmed. No data was repeated.");
    unsigned char number=1U;
    for(std::size_t at=0;at<bytes.size();) {
        if(canceled_&&canceled_())return Fail(error,L"XMODEM operation canceled; receiver cleanup is required.");
        const auto count=(std::min)(std::size_t{128U},bytes.size()-at);
        std::vector<std::uint8_t> frame(133U,0x1AU);
        frame[0]=1U;frame[1]=number;frame[2]=static_cast<std::uint8_t>(~number);
        std::copy_n(bytes.begin()+at,count,frame.begin()+3);
        std::uint16_t crc=0U;
        for(unsigned i=3U;i<131U;++i){crc^=static_cast<std::uint16_t>(frame[i]<<8);
            for(unsigned bit=0;bit<8U;++bit)crc=static_cast<std::uint16_t>((crc&0x8000U)?(crc<<1)^0x1021U:crc<<1);}
        frame[131]=static_cast<std::uint8_t>(crc>>8);frame[132]=static_cast<std::uint8_t>(crc);
        bool ack=false;
        for(unsigned attempt=0;attempt<3U&&!ack;++attempt) {
            reply.clear();
            if(!stream_exchange_(frame,3000U,&reply,error)) {
                if(canceled_&&canceled_())return false;
                continue; // same block/content only; receiver compares duplicates
            }
            if(reply==std::vector<std::uint8_t>{6U})ack=true;
            else if(reply!=std::vector<std::uint8_t>{0x15U})
                return Fail(error,L"XMODEM receiver canceled or returned an invalid control byte.");
        }
        if(!ack)return Fail(error,L"XMODEM block ACK was not confirmed after three identical attempts.");
        at+=count;++number;
        if(progress)progress(2U+static_cast<unsigned>(at*88U/bytes.size()),L"3/5 Write");
    }
    if(progress)progress(92U,L"4/5 END CRC");
    // Exactly one EOT. A lost final ACK cannot authorize another END/erase.
    reply.clear();
    if(!stream_exchange_({4U},5000U,&reply,error)||reply!=std::vector<std::uint8_t>{6U})
        return Fail(error,L"XMODEM EOT/CRC result is not confirmed. EOT was sent once; preserve the report and read live INFO.");
    return true;
}
} // namespace firmware_upgrade::isp
