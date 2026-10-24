#include "uart_host_client.h"
#include "isp_bus_settings.h"
#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>
extern "C" {
#include "bridge_uart.h"
}
#include "isp_device.h"

namespace {
using namespace firmware_upgrade;
std::deque<std::uint8_t> wire_rx;
std::vector<std::uint8_t> wire_tx;
unsigned sends = 0U;
bool stalled = false, isp_echo = false;
std::uint8_t backend_error = 0U;
std::uint16_t tick = 0U;
void Check(bool ok, const char* name) { if (!ok) throw std::runtime_error(name); }
void Put16(std::vector<std::uint8_t>& p, std::uint16_t n) {
    p.push_back(static_cast<std::uint8_t>(n)); p.push_back(static_cast<std::uint8_t>(n >> 8));
}
void Put32(std::vector<std::uint8_t>& p, std::uint32_t n) {
    Put16(p, static_cast<std::uint16_t>(n)); Put16(p, static_cast<std::uint16_t>(n >> 16));
}
std::uint8_t Command(const std::vector<std::uint8_t>& p, std::vector<std::uint8_t>* out = nullptr) {
    std::uint8_t buffer[128]{}; std::uint16_t size = 0U;
    const auto status = Bridge_UartCommand(p.data(), static_cast<std::uint16_t>(p.size()), buffer, &size, tick);
    if (out) out->assign(buffer, buffer + size);
    return status;
}
std::vector<std::uint8_t> Begin(unsigned tx, unsigned rx, unsigned min,
                                 unsigned timeout = 100U, unsigned idle = 5U) {
    std::vector<std::uint8_t> p{0U, 0U, 1U, 0U};
    for (const auto n : {tx, rx, min, timeout, idle}) Put16(p, static_cast<std::uint16_t>(n));
    return p;
}
void Reset() {
    Bridge_UartAbort(); Bridge_UartInit(); tick = 0U;
    wire_rx.clear(); wire_tx.clear(); sends = 0U; stalled = false; isp_echo = false; backend_error = 0U;
    Check(Bridge_UartConfigure(0U,115200U) == 0U,"configure");
}
void Upload(const std::vector<std::uint8_t>& data) {
    for (std::size_t at = 0U; at < data.size();) {
        const auto n = (std::min)(std::size_t{125U}, data.size() - at);
        std::vector<std::uint8_t> p{1U}; Put16(p,static_cast<std::uint16_t>(at));
        p.insert(p.end(),data.begin()+at,data.begin()+at+n);
        Check(Command(p)==0U,"upload"); at+=n;
    }
}
std::pair<unsigned,unsigned> Finish(unsigned limit = 30001U) {
    std::uint8_t status{}; std::uint16_t size{};
    for (unsigned i=0U;i<limit;++i) {
        Bridge_UartPoll(tick);
        if (Bridge_UartTakeResult(&status,&size)) return {status,size};
        tick=static_cast<std::uint16_t>((tick+1U)%60000U);
    }
    throw std::runtime_error("completion bound");
}
void EngineTests() {
    Reset(); Check(Command(Begin(513U,1U,1U))==3U,"reject oversized");
    Check(Command(Begin(1U,2U,1U,100U,0U))==3U,"variable RX requires idle bound");
    Check(Command(Begin(0U,0U,0U))==3U,"empty rejected");
    for (unsigned size=1U;size<=512U;++size) {
        Reset(); std::vector<std::uint8_t> data(size);
        for (unsigned i=0;i<size;++i) data[i]=static_cast<std::uint8_t>(i);
        Check(Command(Begin(size,size,size))==0U,"begin"); Upload(data);
        Check(sends==0U,"no physical side effects during staging");
        Check(Command({2U})==255U,"execute pending");
        Check(Command({2U})==1U,"duplicate EXEC cannot resend while active");
        const auto result=Finish(); Check(result.first==0U&&result.second==size,"bounded receive");
        Check(wire_tx==data&&sends==1U,"one complete physical packet");
        std::vector<std::uint8_t> received;
        for (unsigned at=0U;at<size;) {
            const auto n=(std::min)(125U,size-at);
            std::vector<std::uint8_t> p{3U},out; Put16(p,static_cast<std::uint16_t>(at)); p.push_back(static_cast<std::uint8_t>(n));
            Check(Command(p,&out)==0U&&out.size()==n+2U,"fetch");
            received.insert(received.end(),out.begin()+2,out.end()); at+=n;
        }
        Check(received==data,"all 512-byte packet extents match");
        Check(Command({2U})==10U&&sends==1U,"completed EXEC cannot retransmit");
        wire_rx.push_back(0x43U); Check(Command({5U})==0U&&!Bridge_UartReserved(),"release lease");
        Check(wire_rx.size()==1U,"release preserves asynchronous handshake");
    }
    Reset(); Check(Command(Begin(0U,8U,1U))==0U,"RX only");
    Check(Command({2U})==255U,"RX execute"); wire_rx.push_back(0x43U);
    Check(Finish()==std::make_pair(0U,1U),"idle completes variable short reply");
    Reset(); Check(Command(Begin(0U,1U,1U))==0U,"overflow begin");
    Command({2U}); wire_rx={1U,2U}; Check(Finish().first==8U,"overflow not hidden");
    Reset(); tick=59998U; Check(Command(Begin(0U,8U,8U,4U,0U))==0U,"wrap begin");
    Command({2U}); Check(Finish().first==6U&&tick==2U,"60000-ms wrap deadline");
    Reset(); Check(Command(Begin(1U,0U,0U,5U))==0U,"TX-only begin"); Upload({1U});
    stalled=true; Command({2U}); wire_rx.clear(); Check(Finish().first==6U,"TX stall bounded");
    Reset(); Command(Begin(1U,0U,0U)); tick=5000U; Bridge_UartPoll(tick);
    Check(!Bridge_UartReserved()&&sends==0U,"upload lease expires without send");
    Reset(); Command(Begin(0U,8U,8U)); Command({2U}); backend_error=11U;
    Check(Finish().first==11U,"hardware RX error surfaced");
    Reset(); Command(Begin(0U,8U,8U)); Check(Command({4U})==0U&&!Bridge_UartReserved(),"abort staging");
    Check(Command({4U})==0U,"abort idempotent");
    Reset(); Command(Begin(2U,2U,2U)); Upload({1U}); Check(Command({2U})==10U&&sends==0U,"partial upload cannot send");
    Reset(); Command(Begin(1U,0U,0U)); Upload({0x43U}); Command({2U});
    Check(Finish()==std::make_pair(0U,0U)&&wire_rx.size()==1U,"TX-only leaves asynchronous RX for a later receive");
    Reset(); Command(Begin(0U,1U,1U)); Command({2U}); wire_rx.push_back(0x43U);
    Bridge_UartPoll(tick); tick=6000U; Bridge_UartPoll(tick);
    Check(Bridge_UartReserved()&&Command({5U})==10U,"unconsumed result survives stalled outer TX");
    std::uint8_t status{}; std::uint16_t count{};
    Check(Bridge_UartTakeResult(&status,&count)&&!status&&count==1U,"deferred result retained");
    Bridge_UartPoll(tick); Check(!Bridge_UartReserved(),"consumed stale completion lease released");
}
class Wire final : public host_link::Transport {
public:
    bool IsOpen() const override { return true; }
    bool WriteAll(const std::uint8_t* bytes,std::size_t size,std::uint32_t,std::wstring*) override {
        host_link::Frame request;
        Check(host_link::DecodeFrame(bytes,size,&request)==host_link::DecodeResult::Ok,"real Host Link frame");
        const std::vector<std::uint8_t> raw(bytes,bytes+size);
        if (raw==last_request) { pending=last_reply; return true; }
        auto response=request; response.type=host_link::FrameType::Response; response.payload.clear();
        const auto command=static_cast<host_link::Command>(request.command);
        auto& out=response.payload;
        switch(command) {
        case host_link::Command::GetHostInfo:
            Put16(out,0x101U); out.insert(out.end(),{1U,8U,0U,2U,2U,0U}); Put32(out,20261023U); Put32(out,flags); break;
        case host_link::Command::GetCapabilities:
            Put32(out,flags); Put16(out,128U); out.insert(out.end(),{64U,1U,4U,4U,1U,0U});
            Put32(out,0x16U); Put32(out,2U); Put32(out,1000U); break;
        case host_link::Command::GetPortInfo:
            ++catalog_requests; out={2U,1U,0U,1U}; Put32(out,rate_mask); Put16(out,512U); Put16(out,512U);
            out.insert(out.end(),{'U','A','R','T','1'}); out.resize(32U); break;
        case host_link::Command::SetUartConfig:
            Check(request.payload.size()==8U,"config wire");
            configured_baud=static_cast<std::uint32_t>(request.payload[4])|(static_cast<std::uint32_t>(request.payload[5])<<8U)|
                (static_cast<std::uint32_t>(request.payload[6])<<16U)|(static_cast<std::uint32_t>(request.payload[7])<<24U);
            response.status=static_cast<host_link::Status>(Bridge_UartConfigure(request.payload[0],configured_baud)); out=request.payload; break;
        case host_link::Command::UartTransaction: {
            auto status=Command(request.payload,&out);
            if(status==255U) { const auto result=Finish(); status=static_cast<std::uint8_t>(result.first); Put16(out,static_cast<std::uint16_t>(result.second)); }
            response.status=static_cast<host_link::Status>(status); break;
        }
        case host_link::Command::GetUartStats: out.resize(24U); break;
        default: throw std::runtime_error("unexpected Host Link command");
        }
        last_request=raw; Check(host_link::EncodeFrame(response,&last_reply),"encode Host Link reply");
        if(drop_exec && command==host_link::Command::UartTransaction&&request.payload==std::vector<std::uint8_t>{2U}) drop_exec=false;
        else pending=last_reply;
        return true;
    }
    bool ReadSome(std::vector<std::uint8_t>* out,std::size_t maximum,std::uint32_t,std::wstring*) override {
        const auto n=(std::min)(maximum,pending.size()); out->assign(pending.begin(),pending.begin()+n);
        pending.erase(pending.begin(),pending.begin()+n); return true;
    }
    std::uint32_t flags=0x43BFU;
    std::uint32_t rate_mask=1U,configured_baud=0U;
    unsigned catalog_requests=0U;
    bool drop_exec=false;
    std::vector<std::uint8_t> pending,last_request,last_reply;
};
void ClientTests() {
    Check(static_cast<unsigned>(host_link::Command::SetUartConfig)==BRIDGE_UART_CONFIG_COMMAND&&
        static_cast<unsigned>(host_link::Command::UartTransaction)==BRIDGE_UART_TRANSACTION_COMMAND&&
        static_cast<unsigned>(host_link::Command::GetUartStats)==BRIDGE_UART_STATS_COMMAND&&
        bridge::kFeatureUart==BRIDGE_UART_FEATURE,"PC/firmware UART command and capability parity");
    Reset(); Wire wire; host_link::Client link; link.SetTransport(&wire);
    host_link::HostInfo info; host_link::Capabilities caps; std::wstring error;
    Check(link.Probe(0x101U,&info,&caps,&error),"real client probe");
    std::vector<bridge::Port> ports;
    auto old=caps; old.feature_flags&=~bridge::kFeatureUart;
    Check(bridge::GetPorts(link,old,bridge::Bus::Uart,&ports,&error)&&ports.empty()&&!wire.catalog_requests,"old host no UART query");
    Check(bridge::GetPorts(link,caps,bridge::Bus::Uart,&ports,&error)&&ports.size()==1U,"UART catalog");
    uart_host::Client client; client.SetHostLinkClient(&link);
    Check(!client.Configure({0U,250000U},ports,&error),"unsupported device baud blocked");
    wire.rate_mask=DEVICE_UART_RATE_MASK;
    Check(bridge::GetPorts(link,caps,bridge::Bus::Uart,&ports,&error)&&ports[0].rates==DEVICE_UART_RATE_MASK,"numeric UART rate capability parsed");
    Check(client.Configure({0U,123456U},ports,&error)&&wire.configured_baud==123456U,"custom baud forwarded without rewriting requested value");
    Check(!client.Configure({0U,0U},ports,&error)&&!client.Configure({0U,2000001U},ports,&error),"numeric UART range checked");
    std::uint32_t parsed=0U;
    Check(isp::ParseUartBaud(L"230400",&parsed)&&parsed==230400U&&!isp::ParseUartBaud(L"-1",&parsed)&&
        !isp::ParseUartBaud(L"115200x",&parsed)&&!isp::ParseUartBaud(L"99999999999999999",&parsed),"strict decimal baud parser");
    Check(isp::UartIdleMs(300U)==100U&&isp::UartIdleMs(115200U)==5U&&
        isp::UartTimeoutMs(300U,110U,110U,1000U)==7584U&&
        isp::UartTimeoutMs(115200U,110U,110U,1000U)==1000U,
        "low-rate wire budget/idle floor without changing default115200 scheduling");
    Check(client.Configure({},ports,&error),"client UART configure");
    for(const auto size:{1U,110U,133U,512U}) {
        uart_host::Transaction t; t.transmit.resize(size,0x5AU); t.receive_min=t.receive_max=static_cast<std::uint16_t>(size);
        t.timeout_ms=10U; wire.drop_exec=true; const auto before=sends;
        std::vector<std::uint8_t> rx;
        Check(client.Transfer(t,&rx,&error)&&rx==t.transmit,"production client/C engine roundtrip");
        Check(sends==before+1U,"lost EXEC ACK retries same host sequence without device resend");
    }
    isp_echo=true;
    for(unsigned size=0U;size<=96U;++size) {
        IspPacket request{}; request.sequence=static_cast<unsigned short>(size+1U); request.command=ISP_CMD_ECHO;
        request.length=static_cast<unsigned short>(size);
        for(unsigned i=0U;i<size;++i) request.data[i]=static_cast<unsigned char>(i);
        unsigned char encoded[ISP_FRAME_MAX]{}; const auto n=IspEncode(&request,encoded);
        uart_host::Transaction t; t.transmit.assign(encoded,encoded+n);
        t.receive_min=14U; t.receive_max=ISP_FRAME_MAX;
        std::vector<std::uint8_t> rx;
        Check(client.Transfer(t,&rx,&error),"nested ISP packet through raw bridge");
        IspStream stream{}; IspPacket reply{}; unsigned complete=0U;
        for(const auto value:rx) complete+=IspStreamFeed(&stream,value,&reply);
        Check(complete==1U&&reply.response==1U&&reply.status==0U&&reply.sequence==request.sequence&&
            reply.length==size&&std::equal(reply.data,reply.data+size,request.data),"actual device Transfer core echo/CRC");
    }
    std::vector<std::uint32_t> counters;
    Check(client.ReadStats(&counters,&error)&&counters.size()==6U,"typed separate UART counters");
}
}
extern "C" {
std::uint8_t Bridge_UartBackend_Open(std::uint8_t port,std::uint32_t baud) { return port||!isp::ValidUartBaud(baud) ? 3U:0U; }
std::uint8_t Bridge_UartBackend_Begin(const std::uint8_t* tx,std::uint16_t size,std::uint8_t clear) {
    if(clear) wire_rx.clear();
    if(size) ++sends;
    wire_tx.assign(tx,tx+size);
    if(isp_echo) {
        IspStream stream{}; IspPacket request{},reply{};
        for(const auto value:wire_tx) if(IspStreamFeed(&stream,value,&request)) {
            IspDeviceConfig config{}; IspDevice device{}; IspDeviceInit(&device,&config,nullptr,ISP_MODE_TRANSFER);
            IspDeviceProcess(&device,&request,&reply);
            unsigned char encoded[ISP_FRAME_MAX]{}; const auto n=IspEncode(&reply,encoded);
            wire_rx.insert(wire_rx.end(),encoded,encoded+n);
        }
    } else wire_rx.insert(wire_rx.end(),wire_tx.begin(),wire_tx.end());
    return 0U;
}
std::uint8_t Bridge_UartBackend_Get(std::uint8_t* value) {
    if(wire_rx.empty()) return 0U; *value=wire_rx.front(); wire_rx.pop_front(); return 1U;
}
std::uint8_t Bridge_UartBackend_TxIdle(void) { return static_cast<std::uint8_t>(!stalled); }
std::uint8_t Bridge_UartBackend_Error(void) { return backend_error; }
void Bridge_UartBackend_Abort(void) { wire_rx.clear(); }
void Bridge_UartBackend_Stats(std::uint8_t* output) { std::fill(output,output+24U,std::uint8_t{0U}); }
}
int main() {
    try { EngineTests(); ClientTests(); std::cout<<"PASS bridge UART C engine + production PC client + actual ISP Transfer framing/engine; hardware NOT RUN\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
