#pragma once
#include "host_link_client.h"

namespace firmware_upgrade::bridge {
constexpr std::uint32_t kFeatureI2c = 0x100U;
constexpr std::uint32_t kFeaturePortCatalog = 0x200U;
enum class Bus : std::uint8_t { Can = 0U, I2c = 1U };
struct Port {
    Bus bus = Bus::Can;
    std::uint8_t id = 0U;
    std::uint8_t flags = 0U;
    std::uint32_t rates = 0U;
    std::uint16_t maximum_tx = 0U;
    std::uint16_t maximum_rx = 0U;
    std::wstring label;
};
bool GetPorts(host_link::Client& client, const host_link::Capabilities& caps,
              Bus bus, std::vector<Port>* ports, std::wstring* error);
// Shared read-only acceptance for UI preflight and worker port selection.
bool CanPortSupports(const host_link::Capabilities& caps,const std::vector<Port>& ports,
                     std::uint8_t id,std::uint8_t nominal_preset,bool fd);
bool SelectCanPort(host_link::Client& client,
                   const host_link::Capabilities& caps,
                   const std::vector<Port>& ports, std::uint8_t id,
                   std::wstring* error);
} // namespace firmware_upgrade::bridge
