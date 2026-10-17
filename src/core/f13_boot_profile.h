#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "image_policy.h"

#include "can_host_client.h"

namespace firmware_upgrade::f13_boot {

constexpr std::uint32_t kRequestId = 0x700U;
constexpr std::uint32_t kResponseId = 0x701U;
constexpr std::uint32_t kResponseMask = 0x7FFU;
constexpr std::uint8_t kProtocolVersion = 5U;
constexpr std::uint8_t kImageFormatVersion = 6U;
constexpr std::uint16_t kTargetId = 0xF13BU;
constexpr std::uint32_t kApplicationStart = 0x2000U;
constexpr std::uint32_t kApplicationHeaderSize = 0x200U;
constexpr std::uint32_t kMinimumUpdateImageSize =
    kApplicationHeaderSize + 4U;
constexpr std::uint32_t kMaximumUpdateImageSize = 0x1E000U;
constexpr std::uint32_t kManifestOffset = 0x100U;
constexpr std::uint32_t kManifestMagic = 0x004E4143U; // ASCII CAN + NUL.
constexpr std::uint8_t kManifestSize = IP_CORE_SIZE;
constexpr std::uint32_t kDataBytes = 4U; // Flash word / END detail, not CAN block payload.
constexpr std::uint32_t kBlockBytes = 64U;
constexpr std::uint32_t kMaximumDataOffset = 0x00FFFFFCU;
constexpr std::uint32_t kEraseBoundaryBytes = 1024U;

enum class Command : std::uint8_t {
    Hello = 0x01U,
    Start = 0x02U,
    BlockBegin = 0x0BU,
    BlockCommit = 0x0CU,
    End = 0x04U,
    Abort = 0x05U,
    Enter = 0x06U,
    Info = 0x07U,
    Metadata = 0x08U,
    Seal = 0x09U,
    MetadataReset = 0x0AU,
    Verify = 0x0DU,
    ForceStart = 0x0EU,
};

enum class DeviceState : std::uint8_t {
    Application = 0x01U,
    Bootloader = 0x02U,
};

enum class InfoPage : std::uint8_t {
    Core = 0x00U,
    BootVersion = 0x01U,
    BootBuild = 0x02U,
    AppVersion = 0x03U,
    AppBuild = 0x04U,
    ApplicationStart = 0x05U,
    ApplicationCapacity = 0x06U,
    CodeFlashSize = 0x07U,
    BootReservedSize = 0x08U,
    EraseBlockSize = 0x09U,
    CanCapabilities = 0x0AU,
    NominalBitrateMask = 0x0BU,
    DataBitrateMask = 0x0CU,
    RequestId = 0x0DU,
    ResponseId = 0x0EU,
    Part0 = 0x0FU,
    Part1 = 0x10U,
    Part2 = 0x11U,
    Part3 = 0x12U,
    Family0 = 0x13U,
    Family1 = 0x14U,
    Family2 = 0x15U,
    InstalledAppSize = 0x16U,
    MinimumImageSize = 0x17U,
    UpdatePolicy = 0x18U,
    IrqAbi = 0x19U,
    Block = 0x1AU,
    Health = 0x1BU,
    Diagnostics = 0x1CU,
    VersionPolicy = 0x1DU,
};

enum class Status : std::uint8_t {
    Ok = 0x00U,
    BadCommand = 0x01U,
    BadState = 0x02U,
    BadSequence = 0x03U,
    BadSize = 0x04U,
    FlashError = 0x05U,
    CrcError = 0x06U,
    BadProtocol = 0x07U,
    BadTarget = 0x08U,
    BadImage = 0x09U,
    Metadata = 0x0AU,
    Policy = 0x0BU,
    IrqAbi = 0x0CU,
    Layout = 0x0DU,
    Version = 0x0EU,
};

struct ImageInfo {
    bool force_old_version = false; // Request-local; never persisted in BIN.
    std::uint8_t format = 0U;
    std::uint8_t protocol_version = 0U;
    std::uint8_t manifest_length = 0U;
    std::uint16_t target_id = 0U;
    std::uint32_t version = 0U;
    std::uint32_t image_size = 0U;
    std::uint32_t stored_crc32 = 0U;
    std::uint32_t calculated_crc32 = 0U;
    std::uint32_t application_start = kApplicationStart;
    std::uint32_t application_capacity = kMaximumUpdateImageSize;
    std::uint32_t erase_block_size = kEraseBoundaryBytes;
    std::uint32_t minimum_image_size = kMinimumUpdateImageSize;
};

struct HelloInfo {
    Status status = Status::BadCommand;
    std::uint8_t protocol_version = 0U;
    std::uint8_t image_format = 0U;
    std::uint16_t target_id = 0U;
    std::uint16_t maximum_update_size_kib = 0U;
};

struct TargetInfo {
    std::uint32_t version_policy = 0U; // bit0 valid installed App, bit1 enabled.
    std::uint32_t diagnostics = 0U;
    image_policy::Policy policy;
    DeviceState state = DeviceState::Application;
    std::uint16_t target_id = 0U;
    std::uint8_t protocol_version = 0U;
    std::uint8_t image_format = 0U;
    std::uint32_t boot_version = 0U;
    std::uint32_t boot_build = 0U;
    std::uint32_t app_version = 0U;
    std::uint32_t app_build = 0U;
    std::uint32_t installed_app_size = 0U;
    std::uint32_t application_start = 0U;
    std::uint32_t application_capacity = 0U;
    std::uint32_t code_flash_size = 0U;
    std::uint32_t boot_reserved_size = 0U;
    std::uint32_t erase_block_size = 0U;
    std::uint32_t minimum_image_size = 0U;
    std::uint8_t can_mode_mask = 0U;
    std::uint8_t can_id_format = 0U;
    std::uint8_t checksum_type = 0U;
    std::uint8_t protocol_id = 0U;
    std::uint32_t nominal_bitrate_mask = 0U;
    std::uint32_t data_bitrate_mask = 0U;
    std::uint32_t request_id = 0U;
    std::uint32_t response_id = 0U;
    std::uint16_t block_bytes = 0U;
    std::uint16_t block_data_id = 0U;
    std::wstring mcu_part;
    std::wstring mcu_family;
};

struct Ack {
    Command command = Command::Hello;
    Status status = Status::BadCommand;
    std::uint16_t sequence = 0U;
    std::uint32_t value = 0U;
};

std::uint32_t Crc32(const std::uint8_t* data, std::size_t length);

bool ParseUpdateImage(const std::vector<std::uint8_t>& image,
                      ImageInfo* info,
                      std::wstring* error);

bool BuildHelloRequest(can_host::Frame* frame, std::wstring* error);
bool BuildEnterRequest(can_host::Frame* frame, std::wstring* error);
bool BuildInfoRequest(InfoPage page,
                      can_host::Frame* frame,
                      std::wstring* error);
bool BuildStartRequest(const ImageInfo& image,
                       can_host::Frame* frame,
                       std::wstring* error);
bool BuildBlockRequest(Command command, std::uint32_t image_offset,
                      std::uint32_t crc32,
                      can_host::Frame* frame,
                      std::wstring* error);
bool BuildSimpleRequest(Command command,
                        can_host::Frame* frame,
                        std::wstring* error);

bool ParseHelloResponse(const can_host::Frame& frame,
                        HelloInfo* info,
                        std::wstring* error);
bool ParseEnterResponse(const can_host::Frame& frame,
                        Ack* ack,
                        std::wstring* error);
bool ParseInfoResponse(InfoPage expected_page,
                       const can_host::Frame& frame,
                       DeviceState* state,
                       std::uint32_t* value,
                       std::wstring* error);
bool ParseAckResponse(Command expected_command,
                      const can_host::Frame& frame,
                      Ack* ack,
                      std::wstring* error);

class UpdateClient {
public:
    virtual ~UpdateClient() = default;
    virtual bool StageMetadata(const std::vector<std::uint8_t>& image,
        const std::function<bool()>& cancelled, std::wstring* error) = 0;
    virtual bool Start(const ImageInfo& image,
                       Ack* ack,
                       std::wstring* error) = 0;
    virtual bool SendBlock(
        std::uint32_t image_offset,
        const std::vector<std::uint8_t>& data,
        Ack* ack,
        std::wstring* error) = 0;
    virtual bool End(std::uint32_t expected_crc32,
                     std::uint32_t expected_byte_count,
                     Ack* ack,
                     std::wstring* error) = 0;
    virtual bool Abort(Ack* ack, std::wstring* error) = 0;
};

class Client final : public UpdateClient {
public:
    void SetCanClient(can_host::Client* client);
    bool SetCanIdentifiers(std::uint32_t request_id,
                           std::uint32_t response_id,
                           bool extended,
                           std::wstring* error);
    bool SetApplicationStart(std::uint32_t application_start,
                             std::wstring* error);
    bool SetTargetLayout(std::uint32_t application_start,
                         std::uint32_t application_capacity,
                         std::uint32_t erase_block_size,
                         std::wstring* error);
    void SetTransactionPolicy(std::uint16_t normal_timeout_ms,
                              std::uint16_t erase_timeout_ms,
                              std::uint16_t end_timeout_ms,
                              std::uint8_t retry_count);

    bool Hello(HelloInfo* info, std::wstring* error);
    bool GetTargetInfo(TargetInfo* info, std::wstring* error);
    bool GetAppHealth(std::uint32_t* health, std::wstring* error);
    bool GetDiagnostics(std::uint32_t* flags, std::wstring* error);
    bool VerifyInstalled(Ack* ack, std::wstring* error);
    bool EnterBootloader(std::wstring* error);
    bool StageMetadata(const std::vector<std::uint8_t>& image,
        const std::function<bool()>& cancelled, std::wstring* error) override;
    bool Start(const ImageInfo& image,
               Ack* ack,
               std::wstring* error) override;
    bool SendBlock(std::uint32_t image_offset,
                  const std::vector<std::uint8_t>& data,
                  Ack* ack,
                  std::wstring* error) override;
    bool End(std::uint32_t expected_crc32,
             std::uint32_t expected_byte_count,
             Ack* ack,
             std::wstring* error) override;
    bool Abort(Ack* ack, std::wstring* error) override;

private:
    bool Exchange(const can_host::Frame& request,
                  std::uint8_t expected_dlc,
                  std::uint16_t timeout_ms,
                  can_host::Frame* response,
                  std::wstring* error);
    static bool RequireOk(const Ack& ack, std::wstring* error);

    can_host::Client* client_ = nullptr;
    std::uint32_t request_id_ = kRequestId;
    std::uint32_t response_id_ = kResponseId;
    std::uint16_t block_data_id_ = 0U;
    bool extended_id_ = false;
    std::uint32_t application_start_ = kApplicationStart;
    std::uint32_t application_capacity_ = kMaximumUpdateImageSize;
    std::uint32_t erase_block_size_ = kEraseBoundaryBytes;
    std::uint16_t normal_timeout_ms_ = 500U;
    std::uint16_t erase_timeout_ms_ = 3000U;
    std::uint16_t end_timeout_ms_ = 5000U;
    std::uint8_t retry_count_ = 1U;
};

} // namespace firmware_upgrade::f13_boot
