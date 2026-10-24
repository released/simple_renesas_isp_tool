#include "bridge_port_catalog.h"
#include "../../firmware/common/device_uart_rates.h"
#include <algorithm>

namespace firmware_upgrade::bridge {
namespace {
bool Fail(std::wstring* error, const wchar_t* message) {
    if (error != nullptr) *error = message;
    return false;
}
std::uint16_t Le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
}

bool CanPortSupports(const host_link::Capabilities& caps,const std::vector<Port>& ports,
                     std::uint8_t id,std::uint8_t nominal_preset,bool fd) {
    if(nominal_preset>=32U) return false;
    for(const auto& p:ports) if(p.bus==Bus::Can && p.id==id && (p.flags&1U) &&
        (p.rates&(1UL<<nominal_preset)) && (!fd || !(caps.feature_flags&kFeaturePortCatalog) || (p.flags&4U))) return true;
    return false;
}
bool GetPorts(host_link::Client& client, const host_link::Capabilities& caps,
              Bus bus, std::vector<Port>* ports, std::wstring* error) {
    if (ports == nullptr || !client.IsHostValidated())
        return Fail(error, L"Probe the bridge before enumerating bus ports.");
    ports->clear();
    if (bus == Bus::Uart && (caps.feature_flags & kFeatureUart) == 0U) return true;
    if ((caps.feature_flags & kFeaturePortCatalog) == 0U) {
        if (bus == Bus::Can && (caps.feature_flags & 3U) != 0U)
            ports->push_back(Port{Bus::Can, 0U, 1U, caps.nominal_bitrate_mask,
                caps.maximum_can_data_bytes, caps.maximum_can_data_bytes,
                L"CAN0 (legacy bridge)"});
        return true;
    }
    std::uint8_t count = 1U;
    for (std::uint8_t index = 0U; index < count; ++index) {
        host_link::Response response;
        if (!client.Request(host_link::Command::GetPortInfo,
                            {static_cast<std::uint8_t>(bus), index},
                            &response, error)) return false;
        const auto& p = response.payload;
        if (p.size() != 32U || p[0] != static_cast<std::uint8_t>(bus) ||
            p[1] > 16U || (index != 0U && p[1] != count) ||
            (p[3] & ~7U) != 0U)
            return Fail(error, L"Invalid bridge port descriptor.");
        count = p[1];
        if (count == 0U) return true;
        Port port;
        port.bus = bus; port.id = p[2]; port.flags = p[3];
        port.rates = static_cast<std::uint32_t>(p[4]) |
            (static_cast<std::uint32_t>(p[5]) << 8) |
            (static_cast<std::uint32_t>(p[6]) << 16) |
            (static_cast<std::uint32_t>(p[7]) << 24);
        port.maximum_tx = Le16(&p[8]); port.maximum_rx = Le16(&p[10]);
        bool ended = false;
        for (std::size_t c = 12U; c < p.size(); ++c) {
            if (p[c] == 0U) { ended = true; continue; }
            if (ended || p[c] < 32U || p[c] > 126U)
                return Fail(error, L"Invalid bridge port route label.");
            port.label += static_cast<wchar_t>(p[c]);
        }
        if (port.label.empty() || port.id == 0xFFU ||
            (port.rates == 0U && (port.flags & 1U) != 0U) ||
            port.maximum_tx == 0U || port.maximum_rx == 0U ||
            (bus == Bus::Can && ((port.flags & 2U) != 0U ||
             (port.rates & ~caps.nominal_bitrate_mask) != 0U ||
             port.maximum_tx > caps.maximum_can_data_bytes ||
             port.maximum_rx > caps.maximum_can_data_bytes)) ||
            (bus == Bus::I2c &&
             ((port.flags & 4U) != 0U || (port.rates & ~3U) != 0U ||
              port.maximum_tx > 512U || port.maximum_rx > 512U)) ||
            (bus == Bus::Uart &&
             ((port.flags & ~1U) != 0U || (port.rates & ~DEVICE_UART_RATE_MASK) != 0U ||
              port.maximum_tx > 512U || port.maximum_rx > 512U)) ||
            std::any_of(ports->begin(), ports->end(), [&port](const Port& x) {
                return x.id == port.id;
            })) return Fail(error, L"Inconsistent bridge port capabilities.");
        ports->push_back(std::move(port));
    }
    return true;
}

bool SelectCanPort(host_link::Client& client,
                   const host_link::Capabilities& caps,
                   const std::vector<Port>& ports, std::uint8_t id,
                   std::wstring* error) {
    if (!client.IsHostValidated() ||
        std::none_of(ports.begin(), ports.end(), [id](const Port& p) {
            return p.bus == Bus::Can && p.id == id && (p.flags & 1U) != 0U;
        })) return Fail(error, L"The selected CAN port is not available.");
    if ((caps.feature_flags & kFeaturePortCatalog) == 0U)
        return id == 0U;
    host_link::Response response;
    if (!client.Request(host_link::Command::SelectCanPort, {id}, &response, error))
        return false;
    return response.payload == std::vector<std::uint8_t>{id} ||
        Fail(error, L"CAN port selection acknowledgement mismatch.");
}
} // namespace firmware_upgrade::bridge
