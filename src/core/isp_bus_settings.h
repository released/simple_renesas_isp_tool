#pragma once
#include "../../firmware/common/device_uart_rates.h"
#include <cstdint>
#include <cstddef>
#include <string>

namespace firmware_upgrade::isp {
struct BusConfiguration {
    std::uint8_t port=0U;
    std::uint32_t baud=115200U;
    std::uint8_t address=0x20U; // Selected7-bit address,not a field reported by INFO.
};
inline bool ValidUartBaud(std::uint32_t baud) {
    return baud>=DEVICE_UART_BAUD_MIN&&baud<=DEVICE_UART_BAUD_MAX;
}
inline bool ValidI2cRate(std::uint32_t hz) { return hz==100000U||hz==400000U; }
inline bool ValidI2cAddress(std::uint32_t address) { return address>=8U&&address<=0x77U; }
inline bool SameBus(const BusConfiguration& a,const BusConfiguration& b) {
    return a.port==b.port&&a.baud==b.baud&&a.address==b.address;
}
inline bool ValidBus(std::uint8_t contract,const BusConfiguration& c) {
    return c.port!=0xFFU&&(contract==2U?(ValidI2cRate(c.baud)&&ValidI2cAddress(c.address)):
        ((contract==1U||contract==3U)&&ValidUartBaud(c.baud)));
}
inline bool ParseUartBaud(const std::wstring& text,std::uint32_t* baud) {
    if(!baud||text.empty())return false;
    std::uint32_t value=0U;
    for(const auto c:text) {
        if(c<L'0'||c>L'9'||value>DEVICE_UART_BAUD_MAX/10U)return false;
        value=value*10U+static_cast<unsigned>(c-L'0');
        if(value>DEVICE_UART_BAUD_MAX)return false;
    }
    if(!ValidUartBaud(value))return false;
    *baud=value;return true;
}
inline bool UartRateSupported(std::uint32_t mask,std::uint32_t baud) {
    return ValidUartBaud(baud)&&((mask&DEVICE_UART_RATE_NUMERIC)||
        (baud==115200U&&(mask&DEVICE_UART_RATE_DEFAULT)));
}
inline std::uint16_t UartIdleMs(std::uint32_t baud) {
    const auto gap=ValidUartBaud(baud)?(30000U+baud-1U)/baud:5U;
    return static_cast<std::uint16_t>(gap>5U?gap:5U); // At least three8-N-1 characters.
}
inline std::uint16_t UartTimeoutMs(std::uint32_t baud,std::size_t tx,std::size_t rx,std::uint32_t requested) {
    if(!ValidUartBaud(baud))return 0U;
    const auto wire=(static_cast<std::uint64_t>(tx)+rx)*10000U;
    const auto floor=(wire+baud-1U)/baud+250U;
    const auto result=floor>requested?floor:requested;
    return result<=30000U?static_cast<std::uint16_t>(result):0U;
}
inline std::wstring BusText(std::uint8_t contract,const BusConfiguration& c) {
    if(contract!=2U)return L"UART / "+std::to_wstring(c.baud)+L" / 8-N-1";
    const wchar_t digits[]=L"0123456789ABCDEF";
    std::wstring address=L"0x";address+=digits[c.address>>4U];address+=digits[c.address&15U];
    return L"I2C / "+std::to_wstring(c.baud/1000U)+L" kHz / selected7-bit "+address;
}
}
