#pragma once
#include "../../firmware/common/image_policy.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace firmware_upgrade::image_policy {
struct Policy {
    std::uint8_t role=0, transports=0;
    std::uint32_t irq_abi=0, minimum_boot_version=0;
    std::uint32_t device_uart_baud=0;
};
inline std::uint32_t Read32(const std::uint8_t* p) {
    return p[0] | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
inline bool Parse(const std::uint8_t* metadata, Policy* policy) {
    const auto* p=metadata+IP_OFFSET;
    if (Read32(p)!=IP_MAGIC || (p[4]!=IP_VERSION && p[4]!=IP_DEVICE_UART_VERSION) || p[5]!=IP_SIZE ||
        (p[6]!=IP_ROLE_DEVICE_APP && p[6]!=IP_ROLE_BRIDGE_APP) ||
        !p[7] || (p[7]&~IP_ALL) || !Read32(p+8) || Read32(p+8)==0xFFFFFFFFU ||
        Read32(p+12)==0xFFFFFFFFU) return false;
    const auto uart=Read32(p+16);
    const bool device_uart=p[6]==IP_ROLE_DEVICE_APP && (p[7]&IP_UART);
    if (p[4]==IP_DEVICE_UART_VERSION) {
        if (!device_uart || !uart || uart==0xFFFFFFFFU) return false;
    } else if (device_uart || uart) return false;
    for (std::size_t i=20; i<IP_SIZE; ++i) if (p[i]) return false;
    policy->role=p[6]; policy->transports=p[7];
    policy->irq_abi=Read32(p+8); policy->minimum_boot_version=Read32(p+12);
    policy->device_uart_baud=uart;
    return true;
}
inline std::wstring Transports(std::uint8_t mask) {
    std::wstring s;
    if (mask&IP_CAN) s=L"CAN";
    if (mask&IP_I2C) s+=(s.empty()?L"":L" / ")+std::wstring(L"I2C");
    if (mask&IP_UART) s+=(s.empty()?L"":L" / ")+std::wstring(L"UART");
    return s.empty()?L"NONE":s;
}
inline bool Allows(const Policy& p, std::uint8_t role, std::uint8_t transport, std::wstring* error) {
    if (p.role==role && (transport==IP_CAN || transport==IP_I2C || transport==IP_UART) &&
        (p.transports&transport)) return true;
    if (error) *error=L"Image policy mismatch: image role "+
        std::wstring(p.role==IP_ROLE_BRIDGE_APP?L"HOST BRIDGE APP":L"DEVICE APP")+
        L", allowed "+Transports(p.transports)+L"; selected "+
        std::wstring(role==IP_ROLE_BRIDGE_APP?L"HOST BRIDGE APP":L"DEVICE APP")+
        L" via "+Transports(transport)+L". Update blocked before erase.";
    return false;
}
inline std::wstring DeviceUartText(const Policy& p) {
    return p.device_uart_baud ? L" @"+std::to_wstring(p.device_uart_baud) : L"";
}
inline bool AllowsDeviceUartBaud(const Policy& p, std::uint32_t baud, std::wstring* error) {
    if (!Allows(p,IP_ROLE_DEVICE_APP,IP_UART,error)) return false;
    if (p.device_uart_baud && p.device_uart_baud==baud) return true;
    if (error) *error=L"Device UART baud differs from compile-time BIN metadata. Host Link baud is unrelated.";
    return false;
}
}
