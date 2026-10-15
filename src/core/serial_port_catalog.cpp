#include "serial_port_catalog.h"

#include <windows.h>
#include <setupapi.h>

#include <cstdint>
#include <iterator>
#include <limits>
#include <map>

namespace firmware_upgrade::serial_ports {
namespace {

std::uint32_t PortNumber(const std::wstring& name) {
    if (name.size() < 4U ||
        (name[0] != L'C' && name[0] != L'c') ||
        (name[1] != L'O' && name[1] != L'o') ||
        (name[2] != L'M' && name[2] != L'm')) {
        return 0U;
    }
    std::uint32_t number = 0U;
    for (std::size_t index = 3U; index < name.size(); ++index) {
        if (name[index] < L'0' || name[index] > L'9') {
            return 0U;
        }
        const auto digit = static_cast<std::uint32_t>(name[index] - L'0');
        if (number > ((std::numeric_limits<std::uint32_t>::max)() - digit) / 10U) {
            return 0U;
        }
        number = number * 10U + digit;
    }
    return number;
}

std::vector<std::wstring> PresentPorts() {
    std::vector<wchar_t> names(32768U, L'\0');
    for (;;) {
        const DWORD length = QueryDosDeviceW(
            nullptr, names.data(), static_cast<DWORD>(names.size()));
        if (length != 0U) {
            std::vector<std::wstring> ports;
            for (std::size_t offset = 0U;
                 offset < length && names[offset] != L'\0';) {
                const std::wstring name(names.data() + offset);
                if (PortNumber(name) != 0U) {
                    ports.push_back(name);
                }
                offset += name.size() + 1U;
            }
            return ports;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            names.size() >= 1048576U) {
            break;
        }
        names.resize(names.size() * 2U, L'\0');
    }

    // Preserve the previous scan as a fallback if namespace enumeration fails.
    std::vector<std::wstring> ports;
    for (unsigned int number = 1U; number <= 256U; ++number) {
        const std::wstring name = L"COM" + std::to_wstring(number);
        wchar_t target[1024] = {};
        if (QueryDosDeviceW(name.c_str(), target,
                            static_cast<DWORD>(std::size(target))) != 0U) {
            ports.push_back(name);
        }
    }
    return ports;
}

std::wstring DeviceProperty(HDEVINFO devices, SP_DEVINFO_DATA* device,
                            DWORD property) {
    wchar_t value[1024] = {};
    DWORD type = 0U;
    if (!SetupDiGetDeviceRegistryPropertyW(
            devices, device, property, &type,
            reinterpret_cast<BYTE*>(value), sizeof(value), nullptr) ||
        type != REG_SZ) {
        return {};
    }
    value[std::size(value) - 1U] = L'\0';
    return value;
}

std::vector<PortInfo> DeviceNames() {
    GUID ports_class = {};
    DWORD required = 0U;
    if (!SetupDiClassGuidsFromNameW(L"Ports", &ports_class, 1U, &required)) {
        return {};
    }
    const HDEVINFO devices = SetupDiGetClassDevsW(
        &ports_class, nullptr, nullptr, DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::vector<PortInfo> result;
    SP_DEVINFO_DATA device = {};
    device.cbSize = sizeof(device);
    for (DWORD index = 0U; SetupDiEnumDeviceInfo(devices, index, &device); ++index) {
        const HKEY key = SetupDiOpenDevRegKey(
            devices, &device, DICS_FLAG_GLOBAL, 0U, DIREG_DEV, KEY_QUERY_VALUE);
        if (key == INVALID_HANDLE_VALUE) {
            continue;
        }
        wchar_t port_name[256] = {};
        DWORD type = 0U;
        DWORD bytes = sizeof(port_name);
        const LSTATUS status = RegQueryValueExW(
            key, L"PortName", nullptr, &type,
            reinterpret_cast<BYTE*>(port_name), &bytes);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS || type != REG_SZ) {
            continue;
        }
        port_name[std::size(port_name) - 1U] = L'\0';
        std::wstring friendly = DeviceProperty(devices, &device, SPDRP_FRIENDLYNAME);
        if (friendly.empty()) {
            friendly = DeviceProperty(devices, &device, SPDRP_DEVICEDESC);
        }
        result.push_back({port_name, friendly});
    }
    SetupDiDestroyDeviceInfoList(devices);
    return result;
}

} // namespace

std::wstring PortInfo::DisplayName() const {
    return friendly_name.empty() ? port_name : port_name + L": " + friendly_name;
}

std::vector<PortInfo> BuildCatalog(
    const std::vector<std::wstring>& present_ports,
    const std::vector<PortInfo>& device_names) {
    std::map<std::uint32_t, PortInfo> ports;
    for (const auto& name : present_ports) {
        const std::uint32_t number = PortNumber(name);
        if (number != 0U) {
            ports.emplace(number, PortInfo{L"COM" + std::to_wstring(number), L""});
        }
    }
    for (const auto& device : device_names) {
        const auto found = ports.find(PortNumber(device.port_name));
        if (found != ports.end() && found->second.friendly_name.empty()) {
            found->second.friendly_name = device.friendly_name;
        }
    }
    std::vector<PortInfo> result;
    for (const auto& entry : ports) {
        result.push_back(entry.second);
    }
    return result;
}

std::vector<PortInfo> Enumerate() {
    return BuildCatalog(PresentPorts(), DeviceNames());
}

} // namespace firmware_upgrade::serial_ports
