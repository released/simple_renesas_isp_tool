#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "can_host_client.h"
#include "../../firmware/common/custom_transfer_fd.h"

namespace firmware_upgrade::f13_transfer {

constexpr std::uint32_t kRequestId = 0x710U;
constexpr std::uint32_t kResponseId = 0x711U;
constexpr std::uint32_t kResponseMask = 0x7FFU;
constexpr std::uint8_t kProtocolMajor = 1U;
constexpr std::uint8_t kProtocolMinor = 0U;
constexpr std::uint16_t kF13ReferenceTargetId = 0xF13BU;
constexpr std::size_t kFrameSize = 8U;

enum class Command : std::uint8_t {
    DeviceInfo = 0x01U,
    Ping = 0x02U,
    Echo = 0x03U,
    PatternTest = 0x04U,
    GetStats = 0x05U,
    ClearStats = 0x06U,
    ErrorResponse = 0x7FU,
};

enum class ErrorStatus : std::uint8_t {
    None = 0x00U,
    BadFormat = 0x01U,
    BadMagic = 0x02U,
    BadVersion = 0x03U,
    BadSequence = 0x04U,
    CrcError = 0x05U,
    BadCommand = 0x06U,
    BadArgument = 0x07U,
};

struct Response {
    std::uint8_t sequence = 0U;
    Command command = Command::ErrorResponse;
    std::array<std::uint8_t, 3> payload{};
    ErrorStatus error_status = ErrorStatus::None;
    std::uint8_t error_detail = 0U;
};

struct DeviceInfo {
    std::uint8_t protocol_major = 0U;
    std::uint8_t protocol_minor = 0U;
    std::uint16_t target_id = 0U;
    std::string part_number;
    std::uint16_t capabilities = 0U;
};

std::uint8_t Crc8Atm(const std::uint8_t* data, std::size_t length);

bool BuildRequest(std::uint8_t sequence,
                  Command command,
                  const std::array<std::uint8_t, 3>& payload,
                  can_host::Frame* frame,
                  std::wstring* error);

bool ParseResponse(std::uint8_t expected_sequence,
                   Command expected_command,
                   const can_host::Frame& frame,
                   Response* response,
                   std::wstring* error);

class DeviceInfoAssembler {
public:
    bool AddPage(const Response& response, std::wstring* error);
    bool IsComplete() const;
    const DeviceInfo& Value() const;

private:
    DeviceInfo value_;
    std::array<bool, 7> pages_{};
    std::array<char, 8> part_number_{};
};

class Client {
public:
    void SetCanClient(can_host::Client* client);
    void SetCanFd(bool enabled) { fd_ = enabled; }
    bool IsCanFd() const { return fd_; }
    void SetTransactionPolicy(std::uint16_t timeout_ms,
                              std::uint8_t retry_count);

    bool Discover(DeviceInfo* info, std::wstring* error);
    bool Ping(const std::array<std::uint8_t, 3>& token,
              std::wstring* error);
    bool Echo(const std::array<std::uint8_t, 3>& input,
              std::array<std::uint8_t, 3>* output,
              std::wstring* error);
    bool PatternTest(const std::array<std::uint8_t, 3>& input,
                     std::array<std::uint8_t, 3>* output,
                     std::wstring* error);
    bool GetStat(std::uint8_t index,
                 std::uint16_t* value,
                 std::wstring* error);
    bool ClearStats(std::wstring* error);
    bool EchoFd64(const std::array<std::uint8_t, CT_FD64_PAYLOAD>& input,
                  std::wstring* error);

private:
    bool Exchange(can_host::Frame request, std::uint8_t expected_dlc,
                  can_host::Frame* received, std::wstring* error);
    bool Execute(Command command,
                 const std::array<std::uint8_t, 3>& payload,
                 Response* response,
                 std::wstring* error);
    std::uint8_t NextSequence();

    can_host::Client* client_ = nullptr;
    std::uint16_t timeout_ms_ = 500U;
    std::uint8_t retry_count_ = 1U;
    std::uint8_t last_sequence_ = 0U;
    bool fd_ = false;
};

} // namespace firmware_upgrade::f13_transfer
