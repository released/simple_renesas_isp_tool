#pragma once
#include "firmware_update_profile.h"
#include "isp_bus_settings.h"
#include <functional>
#include "../../firmware/common/isp_device/isp_protocol.h"

namespace firmware_upgrade::isp {
struct TargetInfo {
    std::uint8_t contract=1U, transport=IP_UART;
    std::uint8_t mode=0U, capabilities=0U;
    bool valid=false, active=false, ready=false;
    std::uint16_t target=0U;
    std::uint32_t start=0U, capacity=0U, flash=0U, boot=0U, erase=0U;
    std::uint32_t boot_version=0U, boot_build=0U, baud=0U, irq_abi=0U;
    std::uint32_t version=0U, build=0U, size=0U;
    bool version_control=false;
    std::uint8_t address=0x20U; // Bound request address;INFO does not report it.
    std::wstring part, family;
};
using Exchange=std::function<bool(const std::vector<std::uint8_t>&,
    std::uint32_t,std::vector<std::uint8_t>*,std::wstring*)>;
struct UartDiagnostics {
    std::uint32_t rx_errors=0U,rx_drops=0U,tx_timeouts=0U,rx_peak=0U,rx_pending=0U;
};
class Client {
public:
    void SetExchange(Exchange exchange);
    void SetCancelCheck(std::function<bool()> canceled);
    // Raw Xmodem byte exchanges are worker-owned, not ISP-framed requests.
    void SetStreamExchange(Exchange exchange);
    void SetBusConfiguration(std::uint8_t contract,const BusConfiguration& configuration);
    void Reset();
    bool Request(std::uint8_t command,const std::vector<std::uint8_t>& payload,
        std::vector<std::uint8_t>* reply,std::wstring* error,std::uint32_t timeout=1000U);
    bool Discover(TargetInfo* target,std::wstring* error);
    bool ReadMetadata(std::vector<std::uint8_t>* bytes,std::wstring* error);
    bool TransferTest(std::wstring* error);
    bool ReadDiagnostics(const TargetInfo& target,UartDiagnostics* out,std::wstring* error);
    bool Program(const std::vector<std::uint8_t>& image,bool force,
        std::uint32_t approved_version,std::function<void(unsigned,const wchar_t*)> progress,
        std::wstring* error);
private:
    bool Stream(const std::vector<std::uint8_t>& bytes,std::function<void(unsigned,const wchar_t*)> progress,std::wstring* error);
    Exchange exchange_;
    Exchange stream_exchange_;
    std::function<bool()> canceled_;
    std::uint16_t sequence_=0U;
    std::uint8_t contract_=0U;
    BusConfiguration configuration_;
};
bool ParseInfo(const std::vector<std::uint8_t>& bytes,TargetInfo* target,std::wstring* error);
bool ParseDiagnostics(const std::vector<std::uint8_t>& bytes,UartDiagnostics* out,std::wstring* error);
bool Matches(const firmware_update::ImageInfo& image,const TargetInfo& target,std::wstring* error);
bool ValidateInstalledMetadata(const std::vector<std::uint8_t>& bytes,const TargetInfo& target,std::wstring* error);
} // namespace firmware_upgrade::isp
