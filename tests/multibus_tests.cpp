#include "bridge_port_catalog.h"
#include "g16_i2c_reference.h"
#include <algorithm>
#include <deque>
#include <iostream>

namespace {
using namespace firmware_upgrade;
int failures = 0;
void Check(bool condition, const char* name) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
void Le16(std::vector<std::uint8_t>& p, std::uint16_t n) {
    p.push_back(static_cast<std::uint8_t>(n)); p.push_back(static_cast<std::uint8_t>(n >> 8));
}
void Le32(std::vector<std::uint8_t>& p, std::uint32_t n) {
    Le16(p, static_cast<std::uint16_t>(n)); Le16(p, static_cast<std::uint16_t>(n >> 16));
}
void Be32(std::vector<std::uint8_t>& p, std::uint32_t n) {
    for (int shift = 24; shift >= 0; shift -= 8) p.push_back(static_cast<std::uint8_t>(n >> shift));
}
std::uint16_t U16(const std::vector<std::uint8_t>& p, std::size_t i) {
    return static_cast<std::uint16_t>(p.at(i) | (p.at(i + 1U) << 8));
}
std::vector<std::uint8_t> Signature(std::uint8_t count = 1U) {
    std::vector<std::uint8_t> data = {count, 2U};
    const std::string label = "RL78_G16_FPB";
    data.insert(data.end(), label.begin(), label.end());
    data.resize(15U, 0U); data.insert(data.end(), {1U, 0U, 1U, 0U});
    return g16_i2c::EncodePacket(0x81U, 0x3AU, data);
}
std::vector<std::uint8_t> Area(std::uint32_t start = 0x1000U,
                               std::uint32_t end = 0x7FFFU) {
    std::vector<std::uint8_t> data = {0U};
    Be32(data, start); Be32(data, end); Be32(data, 1024U);
    Be32(data, 256U); Be32(data, 256U);
    return g16_i2c::EncodePacket(0x81U, 0x3BU, data);
}
class BridgeMock final : public host_link::Transport {
public:
    bool IsOpen() const override { return true; }
    bool WriteAll(const std::uint8_t* data, std::size_t length,
                  std::uint32_t, std::wstring*) override {
        raw.emplace_back(data, data + length);
        host_link::Frame request;
        Check(host_link::DecodeFrame(data, length, &request) == host_link::DecodeResult::Ok,
              "mock decodes real Host Link client frame");
        requests.push_back(request);
        host_link::Frame response = request; response.type = host_link::FrameType::Response;
        response.payload.clear();
        auto& out = response.payload;
        const auto& p = request.payload;
        switch (static_cast<host_link::Command>(request.command)) {
        case host_link::Command::GetHostInfo:
            Le16(out, 0x0202U); out.insert(out.end(), {1U,0U,0U,2U,2U,0U});
            Le32(out, 1U); Le32(out, flags); break;
        case host_link::Command::GetCapabilities:
            Le32(out, flags); Le16(out, 128U);
            out.insert(out.end(), {static_cast<std::uint8_t>((flags & 3U) ? 64U : 0U),
                1U,1U,4U,static_cast<std::uint8_t>((flags & 3U) ? 1U : 0U),0U});
            Le32(out, 0x16U); Le32(out, 2U); Le32(out, 1000U); break;
        case host_link::Command::GetPortInfo: {
            const bool i2c = p.at(0) == 1U;
            const auto count = i2c ? i2c_count : can_count;
            out = {p[0], count, static_cast<std::uint8_t>(duplicate_ids ? 0U : p.at(1)),
                static_cast<std::uint8_t>(i2c ? 3U : 5U)};
            Le32(out, i2c ? 3U : 0x16U); Le16(out, i2c ? 512U : 64U);
            Le16(out, i2c ? 512U : 64U);
            const std::string label = i2c ? "Mock IIC route" : "Mock CAN route";
            out.insert(out.end(), label.begin(), label.end()); out.resize(32U, 0U);
            if (bad_label) out[31] = 'X';
            break;
        }
        case host_link::Command::SelectCanPort:
        case host_link::Command::SetI2cConfig: out = p; break;
        case host_link::Command::I2cTransaction:
            if (p.at(0) == 0U) {
                Check(p.size() == 12U && p[2] == 9U, "BEGIN unshifted address and wire size");
                tx.clear(); rx_size = U16(p,6U); tx_size = U16(p,4U);
            } else if (p[0] == 1U) {
                Check(U16(p,1U) == tx.size() && p.size() <= 128U, "contiguous upload chunks");
                tx.insert(tx.end(), p.begin() + 3U, p.end());
            } else if (p[0] == 2U) {
                if (drop_execute_once) { drop_execute_once = false; return true; }
                Check(tx.size() == tx_size, "execute only complete staged TX");
                physical.push_back(tx);
                if (execute_status != host_link::Status::Ok) response.status = execute_status;
                else {
                    rx.assign(rx_size, 0U);
                    for (std::size_t i=0U; i<rx.size(); ++i) rx[i]=static_cast<std::uint8_t>(i);
                    if (g16) {
                        if (tx == std::vector<std::uint8_t>{0x55U}) rx = {0xC3U};
                        else if (tx.size() >= 6U && tx[3] == 0U)
                            rx = g16_i2c::EncodePacket(0x81U, 0U, {0U});
                        else if (tx.size() >= 6U && tx[3] == 0x3AU) rx = Signature(area_count);
                        else if (tx.size() >= 6U && tx[3] == 0x3BU) rx = Area();
                    }
                    Le16(out, static_cast<std::uint16_t>(rx.size() + (bad_completion ? 1U : 0U)));
                }
            } else if (p[0] == 3U) {
                const auto offset = U16(p,1U); const auto count = p.at(3);
                Check(offset + count <= rx.size(), "bounded receive fetch");
                Le16(out, static_cast<std::uint16_t>(offset + (bad_fetch ? 1U : 0U)));
                out.insert(out.end(), rx.begin()+offset, rx.begin()+offset+count);
            } else if (p[0] == 4U) { ++aborts; tx.clear(); rx.clear(); }
            break;
        default: response.status = host_link::Status::BadCommand; break;
        }
        std::vector<std::uint8_t> encoded;
        Check(host_link::EncodeFrame(response, &encoded), "mock encodes valid response");
        incoming.push_back(std::move(encoded)); return true;
    }
    bool ReadSome(std::vector<std::uint8_t>* out, std::size_t,
                  std::uint32_t, std::wstring* error) override {
        if (incoming.empty()) { *error = L"Read timeout."; return false; }
        *out = std::move(incoming.front()); incoming.pop_front(); return true;
    }
    std::uint32_t flags = 0x303U;
    std::uint8_t can_count = 2U, i2c_count = 3U, area_count = 1U;
    bool duplicate_ids = false, bad_label = false, drop_execute_once = false;
    bool bad_fetch = false, bad_completion = false, g16 = false;
    host_link::Status execute_status = host_link::Status::Ok;
    unsigned aborts = 0U;
    std::vector<std::vector<std::uint8_t>> raw, physical;
    std::vector<host_link::Frame> requests;
private:
    std::uint16_t tx_size = 0U, rx_size = 0U;
    std::vector<std::uint8_t> tx, rx;
    std::deque<std::vector<std::uint8_t>> incoming;
};
struct Fixture {
    BridgeMock transport;
    host_link::Client link;
    host_link::Capabilities caps;
    std::vector<bridge::Port> ports;
    i2c_host::Client i2c;
    std::wstring error;
    Fixture() {
        link.SetTransport(&transport);
        Check(link.Probe(0U, nullptr, &caps, &error), "generic non-F24 bridge probe");
        Check(bridge::GetPorts(link,caps,bridge::Bus::I2c,&ports,&error), "I2C multi-port catalog");
        i2c.SetHostLinkClient(&link);
        Check(i2c.Configure({2U,0U},ports,&error), "select advertised I2C port2");
    }
};
void TestPorts() {
    Fixture f; std::vector<bridge::Port> ports;
    Check(f.ports.size() == 3U && f.ports[2].id == 2U, "no family-guessed port count");
    Check(bridge::GetPorts(f.link,f.caps,bridge::Bus::Can,&ports,&f.error) && ports.size()==2U,
          "CAN two-port catalog");
    Check(bridge::SelectCanPort(f.link,f.caps,ports,1U,&f.error), "select nonzero CAN port");
    const auto before = f.transport.raw.size();
    Check(!bridge::SelectCanPort(f.link,f.caps,ports,9U,&f.error) && before==f.transport.raw.size(),
          "invalid CAN port causes no UART traffic");
    f.transport.duplicate_ids=true;
    Check(!bridge::GetPorts(f.link,f.caps,bridge::Bus::I2c,&ports,&f.error), "duplicate port IDs rejected");
    f.transport.duplicate_ids=false; f.transport.bad_label=true;
    Check(!bridge::GetPorts(f.link,f.caps,bridge::Bus::I2c,&ports,&f.error), "nonzero label padding rejected");
    f.transport.bad_label=false; f.transport.i2c_count=17U;
    Check(!bridge::GetPorts(f.link,f.caps,bridge::Bus::I2c,&ports,&f.error), "catalog bounded16");
    f.transport.i2c_count=0U;
    Check(bridge::GetPorts(f.link,f.caps,bridge::Bus::I2c,&ports,&f.error) && ports.empty(), "zero ports");
    auto legacy=f.caps; legacy.feature_flags=3U;
    const auto legacy_before=f.transport.raw.size();
    Check(bridge::GetPorts(f.link,legacy,bridge::Bus::Can,&ports,&f.error) && ports.size()==1U &&
          bridge::SelectCanPort(f.link,legacy,ports,0U,&f.error) && legacy_before==f.transport.raw.size(),
          "legacy CAN0 no new commands");
    Check(bridge::GetPorts(f.link,legacy,bridge::Bus::I2c,&ports,&f.error) && ports.empty(), "legacy has no I2C");
    f.transport.flags=0x300U; f.link.BeginSession();
    Check(f.link.Probe(0U,nullptr,&f.caps,&f.error) && f.caps.maximum_can_data_bytes==0U,
          "I2C-only bridge is accepted");
}
void TestTransfers() {
    Fixture f; i2c_host::Transaction t; t.address=9U; t.receive_length=512U;
    std::vector<std::uint8_t> rx;
    for (unsigned i=0U; i<256U; ++i) t.transmit.push_back(static_cast<std::uint8_t>(i));
    t.transmit=g16_i2c::EncodePacket(1U,0x13U,t.transmit); t.repeated_start=true;
    Check(t.transmit.size()==262U && f.i2c.Transfer(t,&rx,&f.error) && rx.size()==512U &&
          f.transport.physical.back()==t.transmit, "262-byte packet assembled once;512-byte RX");
    const auto before=f.transport.raw.size();
    t.address=0x80U;
    Check(!f.i2c.Transfer(t,&rx,&f.error) && before==f.transport.raw.size(), "shifted address rejected before traffic");
    t.address=9U; t.transmit.resize(513U);
    Check(!f.i2c.Transfer(t,&rx,&f.error) && before==f.transport.raw.size(), "oversize rejected before traffic");
    t.transmit={0x55U}; t.receive_length=1U; t.response_gap_ms=1U;
    Check(!f.i2c.Transfer(t,&rx,&f.error) && before==f.transport.raw.size(), "repeat plus gap rejected");
    t.response_gap_ms=0U; f.transport.drop_execute_once=true;
    const auto physical_before=f.transport.physical.size();
    Check(f.i2c.Transfer(t,&rx,&f.error) && f.transport.physical.size()==physical_before+1U,
          "lost EXEC ack gets bounded retry");
    std::vector<std::vector<std::uint8_t>> executes;
    for (std::size_t i=before; i<f.transport.requests.size(); ++i)
        if (f.transport.requests[i].command==0x41U && f.transport.requests[i].payload==std::vector<std::uint8_t>{2U})
            executes.push_back(f.transport.raw[i]);
    Check(executes.size()==2U && executes[0]==executes[1], "EXEC retry byte-identical sequence/frame");
    for (auto status : {host_link::Status::I2cNack, host_link::Status::I2cArbitrationLost,
                        host_link::Status::I2cBusError, host_link::Status::Timeout}) {
        f.transport.execute_status=status;
        Check(!f.i2c.Transfer(t,&rx,&f.error) && rx.empty(), "I2C error exposes no partial RX");
        Check(f.i2c.Abort(&f.error), "cleanup after target error");
    }
    f.transport.execute_status=host_link::Status::Ok;
    f.transport.bad_completion=true;
    Check(!f.i2c.Transfer(t,&rx,&f.error) && rx.empty(), "wrong completion size blocked");
    Check(f.i2c.Abort(&f.error), "completion cleanup"); f.transport.bad_completion=false;
    f.transport.bad_fetch=true;
    Check(!f.i2c.Transfer(t,&rx,&f.error) && rx.empty(), "wrong FETCH offset blocked");
    Check(f.i2c.Abort(&f.error), "fetch cleanup"); f.transport.bad_fetch=false;
    bool cancel=true; f.link.SetCancelCheck([&cancel]{return cancel;});
    Check(!f.i2c.Transfer(t,&rx,&f.error), "I2C cancellation bounded");
    cancel=false; Check(f.i2c.Abort(&f.error), "cancel cleanup with cancellation disabled");
    Check(f.i2c.Transfer(t,&rx,&f.error), "recovery after negative tests");
}
void TestG16() {
    Fixture f; f.transport.g16=true; g16_i2c::DeviceInfo info;
    Check(g16_i2c::Discover(f.i2c,9U,&info,&f.error) && info.label==L"RL78_G16_FPB" &&
          info.areas.size()==1U && info.areas[0].start==0x1000U && info.areas[0].end==0x7FFFU,
          "G16 actual inquiry/signature/area discovery");
    Check(f.transport.physical.size()==5U, "discovery uses exactly five physical transactions");
    for (const auto& tx : f.transport.physical)
        Check(tx.size()<6U || tx[3]==0U || tx[3]==0x3AU || tx[3]==0x3BU,
              "discovery never sends auth/erase/write/read/app-jump");
    const std::string known="123456789";
    Check(g16_i2c::ImageCrc16(reinterpret_cast<const std::uint8_t*>(known.data()),known.size())==0x31C3U,
          "independent CRC16/XMODEM known vector");
    std::vector<std::uint8_t> image(0x7000U,0xFFU);
    const auto crc=g16_i2c::ImageCrc16(image.data(),image.size()-4U);
    image[image.size()-4U]=static_cast<std::uint8_t>(crc);
    image[image.size()-3U]=static_cast<std::uint8_t>(crc>>8);
    auto check=g16_i2c::InspectImage(image);
    Check(check.available && check.matches && g16_i2c::ImageFitsDevice(info,check,image.size(),&f.error),
          "G16 full geometry BIN CRC16 trailer");
    image[7]^=1U; check=g16_i2c::InspectImage(image);
    Check(!check.matches && !g16_i2c::ImageFitsDevice(info,check,image.size(),&f.error), "G16 corrupt BIN rejected");
    Check(!g16_i2c::ImageFitsDevice(info,g16_i2c::InspectImage(std::vector<std::uint8_t>(32U)),32U,&f.error),
          "G16 sample does not guess raw BIN start or arbitrary size");
    Check(!g16_i2c::ProgrammingBlocker().empty(), "known slave security gaps always block programming");
    std::uint8_t count=0U; auto bad=Signature(0U);
    Check(!g16_i2c::ParseSignature(bad,&info,&count,&f.error), "zero G16 area count rejected");
    bad=Signature(9U); Check(!g16_i2c::ParseSignature(bad,&info,&count,&f.error), "G16 count bounded8");
    bad=Signature(); bad[20]^=1U;
    Check(!g16_i2c::ParseSignature(bad,&info,&count,&f.error), "G16 corrupt metadata SUM rejected");
    g16_i2c::Area area;
    Check(!g16_i2c::ParseArea(Area(0x1000U,0x1FFFFU),&area,&f.error), "G16 32-to16 address truncation blocked");
    Check(!g16_i2c::ParseArea(Area(0x1001U),&area,&f.error), "G16 erase alignment blocked");
    f.transport.area_count=2U;
    Check(!g16_i2c::Discover(f.i2c,9U,&info,&f.error), "G16 overlapping areas blocked");
    Check(g16_i2c::EncodePacket(1U,0U,std::vector<std::uint8_t>(257U)).empty(), "G16 max packet payload256");
}
}
int TestMultiBus() {
    failures=0; TestPorts(); TestTransfers(); TestG16();
    if (failures==0) std::cout << "Multi-bus catalog, staged PC I2C, retries/errors and G16 discovery/CRC16 tests passed\n";
    return failures;
}
