#pragma once

#include <string>
#include <vector>

namespace firmware_upgrade::serial_ports {

struct PortInfo {
    // Only port_name is passed to the serial transport and saved in the INI.
    std::wstring port_name;
    std::wstring friendly_name;

    std::wstring DisplayName() const;
};

// Read-only enumeration: never opens a COM handle or probes an unknown device.
std::vector<PortInfo> Enumerate();

// Shared catalog logic for numeric sorting, deduplication and fallback tests.
std::vector<PortInfo> BuildCatalog(
    const std::vector<std::wstring>& present_ports,
    const std::vector<PortInfo>& device_names);

} // namespace firmware_upgrade::serial_ports
