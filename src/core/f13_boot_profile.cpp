#include "f13_boot_profile.h"

#include "firmware_update_profile.h"

#include <algorithm>
#include <sstream>

namespace firmware_upgrade::f13_boot {

namespace {

std::uint16_t ReadLe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8U));
}

std::uint32_t ReadLe32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

void PutLe16(std::vector<std::uint8_t>* output, std::uint16_t value) {
    output->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void PutLe32(std::vector<std::uint8_t>* output, std::uint32_t value) {
    output->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

bool Fail(const wchar_t* text, std::wstring* error) {
    if (error != nullptr) {
        *error = text;
    }
    return false;
}

bool IsKnownStatus(std::uint8_t value) {
    return value <= static_cast<std::uint8_t>(Status::Version);
}

const wchar_t* StatusText(Status status) {
    switch (status) {
        case Status::Ok: return L"OK";
        case Status::BadCommand: return L"BAD_COMMAND";
        case Status::BadState: return L"BAD_STATE";
        case Status::BadSequence: return L"BAD_SEQUENCE";
        case Status::BadSize: return L"BAD_SIZE";
        case Status::FlashError: return L"FLASH_ERROR";
        case Status::CrcError: return L"CRC_ERROR";
        case Status::BadProtocol: return L"BAD_PROTOCOL";
        case Status::BadTarget: return L"BAD_TARGET";
        case Status::BadImage: return L"BAD_IMAGE";
        case Status::Metadata: return L"METADATA (manifest field or reserved bytes)";
        case Status::Policy: return L"UPDATE_POLICY (role / allowed transport)";
        case Status::IrqAbi: return L"IRQ_ABI";
        case Status::Layout: return L"FLASH_LAYOUT";
        case Status::Version: return L"VERSION_DOWNGRADE (installed App policy rejects this older version)";
        default: return L"UNKNOWN";
    }
}

bool ValidateResponseFrame(const can_host::Frame& frame,
                           std::uint8_t expected_dlc,
                           std::wstring* error) {
    if (frame.id != kResponseId || frame.extended || frame.remote || frame.fd ||
        frame.bit_rate_switch || frame.dlc != expected_dlc ||
        frame.data.size() != expected_dlc) {
        return Fail(L"F13 Boot response CAN frame is invalid.", error);
    }
    return true;
}

bool BuildRequest(const std::vector<std::uint8_t>& data,
                  can_host::Frame* frame,
                  std::wstring* error) {
    if (frame == nullptr || data.empty() || data.size() > 8U) {
        return Fail(L"F13 Boot request is invalid.", error);
    }

    frame->extended = false;
    frame->remote = false;
    frame->fd = false;
    frame->bit_rate_switch = false;
    frame->error_state_indicator = false;
    frame->dlc = static_cast<std::uint8_t>(data.size());
    frame->id = kRequestId;
    frame->timestamp = 0U;
    frame->data = data;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool IsPowerOfTwo(std::uint32_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool AppendAsciiWord(std::uint32_t value, std::wstring* output) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        const std::uint8_t character = static_cast<std::uint8_t>(
            (value >> shift) & 0xFFU);
        if (character == 0U) {
            continue;
        }
        if (character < 0x20U || character > 0x7EU) {
            return false;
        }
        output->push_back(static_cast<wchar_t>(character));
    }
    return true;
}

} // namespace

std::uint32_t Crc32(const std::uint8_t* data, std::size_t length) {
    return firmware_update::Crc32(data, length);
}

bool ParseUpdateImage(const std::vector<std::uint8_t>& image,
                      ImageInfo* info,
                      std::wstring* error) {
    if (info == nullptr) {
        return Fail(L"F13 Boot image information output is null.", error);
    }
    firmware_update::ImageInfo metadata;
    if (!firmware_update::ValidateImage(
            firmware_update::Profile::CanUpgrade, image, &metadata, error)) {
        return false;
    }
    ImageInfo parsed;
    parsed.format = metadata.format;
    parsed.protocol_version = metadata.protocol_version;
    parsed.manifest_length = metadata.manifest_length;
    parsed.target_id = metadata.target_id;
    parsed.version = metadata.version;
    parsed.image_size = metadata.image_size;
    parsed.stored_crc32 = metadata.stored_crc32;
    parsed.calculated_crc32 = metadata.calculated_crc32;
    parsed.application_start = metadata.application_start;
    parsed.application_capacity = metadata.application_capacity;
    parsed.erase_block_size = metadata.erase_block_size;
    parsed.minimum_image_size = metadata.minimum_image_size;
    *info = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool BuildHelloRequest(can_host::Frame* frame, std::wstring* error) {
    return BuildSimpleRequest(Command::Hello, frame, error);
}

bool BuildEnterRequest(can_host::Frame* frame, std::wstring* error) {
    return BuildSimpleRequest(Command::Enter, frame, error);
}

bool BuildInfoRequest(InfoPage page,
                      can_host::Frame* frame,
                      std::wstring* error) {
    if (static_cast<std::uint8_t>(page) >
        static_cast<std::uint8_t>(InfoPage::VersionPolicy)) {
        return Fail(L"F13 identity page is invalid.", error);
    }
    return BuildRequest(
        {static_cast<std::uint8_t>(Command::Info),
         static_cast<std::uint8_t>(page)}, frame, error);
}

bool BuildStartRequest(const ImageInfo& image,
                       can_host::Frame* frame,
                       std::wstring* error) {
    if (image.format != kImageFormatVersion ||
        image.protocol_version != kProtocolVersion ||
        image.target_id == 0U || image.erase_block_size == 0U ||
        image.minimum_image_size <
            firmware_update::kManifestOffset +
                firmware_update::kManifestSize + 4U ||
        image.image_size < image.minimum_image_size ||
        image.image_size > image.application_capacity ||
        image.image_size > kMaximumDataOffset + kDataBytes ||
        (image.image_size % kDataBytes) != 0U) {
        return Fail(L"F13 Boot START image identity is incompatible.", error);
    }

    std::vector<std::uint8_t> data;
    data.reserve(8U);
    data.push_back(static_cast<std::uint8_t>(image.force_old_version ? Command::ForceStart : Command::Start));
    data.push_back(image.protocol_version);
    PutLe16(&data, image.target_id);
    PutLe32(&data, image.image_size);
    return BuildRequest(data, frame, error);
}

bool BuildBlockRequest(Command command, std::uint32_t image_offset,
                      std::uint32_t crc32,
                      can_host::Frame* frame,
                      std::wstring* error) {
    if (image_offset > kMaximumDataOffset ||
        (image_offset % kBlockBytes) != 0U ||
        (command != Command::BlockBegin && command != Command::BlockCommit)) {
        return Fail(L"CAN Upgrade BLOCK command/offset is out of range.", error);
    }

    std::vector<std::uint8_t> request;
    request.reserve(8U);
    request.push_back(static_cast<std::uint8_t>(command));
    request.push_back(static_cast<std::uint8_t>(image_offset & 0xFFU));
    request.push_back(static_cast<std::uint8_t>(
        (image_offset >> 8U) & 0xFFU));
    request.push_back(static_cast<std::uint8_t>(
        (image_offset >> 16U) & 0xFFU));
    PutLe32(&request, crc32);
    return BuildRequest(request, frame, error);
}

bool BuildSimpleRequest(Command command,
                        can_host::Frame* frame,
                        std::wstring* error) {
    if (command != Command::Hello && command != Command::End &&
        command != Command::Abort && command != Command::Enter) {
        return Fail(L"F13 Boot simple request command is invalid.", error);
    }
    return BuildRequest(
        {static_cast<std::uint8_t>(command)}, frame, error);
}

bool ParseHelloResponse(const can_host::Frame& frame,
                        HelloInfo* info,
                        std::wstring* error) {
    HelloInfo parsed;
    if (info == nullptr) {
        return Fail(L"F13 Boot HELLO output is null.", error);
    }
    if (!ValidateResponseFrame(frame, 8U, error) ||
        frame.data[0] != static_cast<std::uint8_t>(Command::Hello) ||
        !IsKnownStatus(frame.data[1])) {
        return Fail(L"F13 Boot HELLO response is invalid.", error);
    }

    parsed.status = static_cast<Status>(frame.data[1]);
    parsed.protocol_version = frame.data[2];
    parsed.image_format = frame.data[3];
    parsed.target_id = ReadLe16(&frame.data[4]);
    parsed.maximum_update_size_kib = ReadLe16(&frame.data[6]);
    *info = parsed;
    if (parsed.protocol_version != kProtocolVersion ||
        parsed.image_format != kImageFormatVersion ||
        parsed.target_id == 0U ||
        parsed.maximum_update_size_kib == 0U) {
        return Fail(L"F13 Boot HELLO identity is incompatible.", error);
    }
    if (parsed.status != Status::Ok) {
        std::wstringstream stream;
        stream << L"F13 Boot HELLO returned "
               << StatusText(parsed.status) << L".";
        if (error != nullptr) {
            *error = stream.str();
        }
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool ParseEnterResponse(const can_host::Frame& frame,
                        Ack* ack,
                        std::wstring* error) {
    if (ack == nullptr) {
        return Fail(L"F13 Boot ENTER output is null.", error);
    }
    if (!ValidateResponseFrame(frame, 2U, error) ||
        frame.data[0] != static_cast<std::uint8_t>(Command::Enter) ||
        !IsKnownStatus(frame.data[1])) {
        return Fail(L"F13 Boot ENTER response is invalid.", error);
    }
    ack->command = Command::Enter;
    ack->status = static_cast<Status>(frame.data[1]);
    ack->sequence = 0U;
    ack->value = 0U;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool ParseInfoResponse(InfoPage expected_page,
                       const can_host::Frame& frame,
                       DeviceState* state,
                       std::uint32_t* value,
                       std::wstring* error) {
    if (state == nullptr || value == nullptr) {
        return Fail(L"F13 identity output is null.", error);
    }
    if (!ValidateResponseFrame(frame, 8U, error) ||
        frame.data[0] != static_cast<std::uint8_t>(Command::Info) ||
        frame.data[1] != static_cast<std::uint8_t>(Status::Ok) ||
        frame.data[2] != static_cast<std::uint8_t>(expected_page) ||
        (frame.data[3] != static_cast<std::uint8_t>(
                              DeviceState::Application) &&
         frame.data[3] != static_cast<std::uint8_t>(
                              DeviceState::Bootloader))) {
        return Fail(L"F13 identity response is invalid.", error);
    }

    *state = static_cast<DeviceState>(frame.data[3]);
    *value = ReadLe32(&frame.data[4]);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool ParseAckResponse(Command expected_command,
                      const can_host::Frame& frame,
                      Ack* ack,
                      std::wstring* error) {
    if (ack == nullptr) {
        return Fail(L"F13 Boot ACK output is null.", error);
    }
    if (!ValidateResponseFrame(frame, 8U, error) ||
        frame.data[0] != static_cast<std::uint8_t>(expected_command) ||
        !IsKnownStatus(frame.data[1])) {
        return Fail(L"F13 Boot ACK response is invalid.", error);
    }

    ack->command = expected_command;
    ack->status = static_cast<Status>(frame.data[1]);
    ack->sequence = ReadLe16(&frame.data[2]);
    ack->value = ReadLe32(&frame.data[4]);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void Client::SetCanClient(can_host::Client* client) {
    client_ = client;
}

bool Client::SetCanIdentifiers(std::uint32_t request_id,
                               std::uint32_t response_id,
                               bool extended,
                               std::wstring* error) {
    const std::uint32_t maximum_id = extended ? 0x1FFFFFFFU : 0x7FFU;
    if (request_id > maximum_id || response_id > maximum_id ||
        request_id == response_id) {
        return Fail(L"CAN Upgrade request/response identifiers are invalid.",
                    error);
    }
    request_id_ = request_id;
    response_id_ = response_id;
    extended_id_ = extended;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool Client::SetApplicationStart(std::uint32_t application_start,
                                 std::wstring* error) {
    return SetTargetLayout(application_start, application_capacity_,
                           erase_block_size_, error);
}

bool Client::SetTargetLayout(std::uint32_t application_start,
                             std::uint32_t application_capacity,
                             std::uint32_t erase_block_size,
                             std::wstring* error) {
    if (application_capacity == 0U ||
        application_capacity > kMaximumDataOffset + kDataBytes ||
        !IsPowerOfTwo(erase_block_size) ||
        (application_start % erase_block_size) != 0U ||
        static_cast<std::uint64_t>(application_start) +
                application_capacity >
            0xFFFFFFFFULL) {
        return Fail(L"CAN Upgrade target layout is invalid.", error);
    }
    application_start_ = application_start;
    application_capacity_ = application_capacity;
    erase_block_size_ = erase_block_size;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void Client::SetTransactionPolicy(std::uint16_t normal_timeout_ms,
                                  std::uint16_t erase_timeout_ms,
                                  std::uint16_t end_timeout_ms,
                                  std::uint8_t retry_count) {
    if (normal_timeout_ms != 0U && erase_timeout_ms != 0U &&
        end_timeout_ms != 0U && retry_count <= 3U) {
        normal_timeout_ms_ = normal_timeout_ms;
        erase_timeout_ms_ = erase_timeout_ms;
        end_timeout_ms_ = end_timeout_ms;
        retry_count_ = retry_count;
    }
}

bool Client::Hello(HelloInfo* info, std::wstring* error) {
    can_host::Frame request;
    can_host::Frame response;
    if (!BuildHelloRequest(&request, error) ||
        !Exchange(request, 8U, normal_timeout_ms_, &response, error) ||
        !ParseHelloResponse(response, info, error)) {
        return false;
    }
    if (info->maximum_update_size_kib !=
        application_capacity_ / 1024U) {
        return Fail(L"CAN Upgrade HELLO capacity does not match device "
                    L"metadata.", error);
    }
    return true;
}

bool Client::GetTargetInfo(TargetInfo* info, std::wstring* error) {
    if (info == nullptr) {
        return Fail(L"F13 identity output is null.", error);
    }

    TargetInfo parsed;
    DeviceState state;
    std::uint32_t value;
    can_host::Frame request;
    can_host::Frame response;

    const InfoPage pages[] = {
        InfoPage::Core,
        InfoPage::BootVersion,
        InfoPage::BootBuild,
        InfoPage::AppVersion,
        InfoPage::AppBuild,
        InfoPage::ApplicationStart,
        InfoPage::ApplicationCapacity,
        InfoPage::CodeFlashSize,
        InfoPage::BootReservedSize,
        InfoPage::EraseBlockSize,
        InfoPage::CanCapabilities,
        InfoPage::NominalBitrateMask,
        InfoPage::DataBitrateMask,
        InfoPage::RequestId,
        InfoPage::ResponseId,
        InfoPage::Part0,
        InfoPage::Part1,
        InfoPage::Part2,
        InfoPage::Part3,
        InfoPage::Family0,
        InfoPage::Family1,
        InfoPage::Family2,
        InfoPage::InstalledAppSize,
        InfoPage::MinimumImageSize,
        InfoPage::UpdatePolicy,
        InfoPage::IrqAbi,
        InfoPage::Block,
        InfoPage::VersionPolicy,
    };
    for (InfoPage page : pages) {
        if (!BuildInfoRequest(page, &request, error) ||
            !Exchange(request, 8U, normal_timeout_ms_, &response, error) ||
            !ParseInfoResponse(page, response, &state, &value, error)) {
            return false;
        }
        if (page == InfoPage::Core) {
            parsed.state = state;
            parsed.target_id = static_cast<std::uint16_t>(value & 0xFFFFU);
            parsed.protocol_version = static_cast<std::uint8_t>(
                (value >> 16U) & 0xFFU);
            parsed.image_format = static_cast<std::uint8_t>(
                (value >> 24U) & 0xFFU);
        } else {
            if (state != parsed.state) {
                return Fail(L"F13 execution state changed during identity read.",
                            error);
            }
            switch (page) {
                case InfoPage::BootVersion:
                    parsed.boot_version = value;
                    break;
                case InfoPage::BootBuild:
                    parsed.boot_build = value;
                    break;
                case InfoPage::AppVersion:
                    parsed.app_version = value;
                    break;
                case InfoPage::AppBuild:
                    parsed.app_build = value;
                    break;
                case InfoPage::ApplicationStart:
                    parsed.application_start = value;
                    break;
                case InfoPage::ApplicationCapacity:
                    parsed.application_capacity = value;
                    break;
                case InfoPage::CodeFlashSize:
                    parsed.code_flash_size = value;
                    break;
                case InfoPage::BootReservedSize:
                    parsed.boot_reserved_size = value;
                    break;
                case InfoPage::EraseBlockSize:
                    parsed.erase_block_size = value;
                    break;
                case InfoPage::CanCapabilities:
                    parsed.can_mode_mask = static_cast<std::uint8_t>(
                        value & 0xFFU);
                    parsed.can_id_format = static_cast<std::uint8_t>(
                        (value >> 8U) & 0xFFU);
                    parsed.checksum_type = static_cast<std::uint8_t>(
                        (value >> 16U) & 0xFFU);
                    parsed.protocol_id = static_cast<std::uint8_t>(
                        (value >> 24U) & 0xFFU);
                    break;
                case InfoPage::NominalBitrateMask:
                    parsed.nominal_bitrate_mask = value;
                    break;
                case InfoPage::DataBitrateMask:
                    parsed.data_bitrate_mask = value;
                    break;
                case InfoPage::RequestId:
                    parsed.request_id = value;
                    break;
                case InfoPage::ResponseId:
                    parsed.response_id = value;
                    break;
                case InfoPage::Part0:
                case InfoPage::Part1:
                case InfoPage::Part2:
                case InfoPage::Part3:
                    if (!AppendAsciiWord(value, &parsed.mcu_part)) {
                        return Fail(L"MCU part metadata is invalid.", error);
                    }
                    break;
                case InfoPage::Family0:
                case InfoPage::Family1:
                case InfoPage::Family2:
                    if (!AppendAsciiWord(value, &parsed.mcu_family)) {
                        return Fail(L"MCU family metadata is invalid.", error);
                    }
                    break;
                case InfoPage::InstalledAppSize:
                    parsed.installed_app_size = value;
                    break;
                case InfoPage::MinimumImageSize:
                    parsed.minimum_image_size = value;
                    break;
                case InfoPage::UpdatePolicy:
                    if ((value&0xFFFFU)!=(IP_DEVICE_VERSION|(IP_SIZE<<8U)))
                        return Fail(L"Unsupported live update policy descriptor.",error);
                    parsed.policy.version=IP_DEVICE_VERSION;
                    parsed.policy.role=static_cast<std::uint8_t>(value>>16U);
                    parsed.policy.transports=static_cast<std::uint8_t>(value>>24U);
                    break;
                case InfoPage::IrqAbi:
                    parsed.policy.irq_abi=value;
                    break;
                case InfoPage::Block:
                    parsed.block_bytes = static_cast<std::uint16_t>(value);
                    parsed.block_data_id = static_cast<std::uint16_t>(value >> 16U);
                    break;
                case InfoPage::VersionPolicy:
                    if(value>3U || value==2U) return Fail(L"Invalid installed version policy.",error);
                    parsed.version_policy=value;
                    parsed.policy.version_control=(value>>1U)&1U;
                    break;
                case InfoPage::Core:
                case InfoPage::Health:
                case InfoPage::Diagnostics:
                    break;
            }
        }
    }

    if (!image_policy::Allows(parsed.policy,IP_ROLE_DEVICE_APP,IP_CAN,error) ||
        (parsed.policy.transports&~IP_ALL) || !parsed.policy.irq_abi || parsed.policy.irq_abi==0xFFFFFFFFU)
        return Fail(L"Live device update policy / IRQ ABI is incompatible.",error);
    if (parsed.target_id == 0U ||
        ((parsed.version_policy&1U) && parsed.app_version>0xFFFFFFU) ||
        parsed.protocol_version != kProtocolVersion ||
        parsed.image_format != kImageFormatVersion ||
        parsed.boot_version == 0xFFFFFFFFU ||
        parsed.boot_build == 0xFFFFFFFFU ||
        parsed.application_capacity == 0U ||
        parsed.application_capacity > kMaximumDataOffset + kDataBytes ||
        parsed.application_start >= parsed.code_flash_size ||
        static_cast<std::uint64_t>(parsed.application_start) +
                parsed.application_capacity >
            parsed.code_flash_size ||
        parsed.boot_reserved_size > parsed.application_start ||
        !IsPowerOfTwo(parsed.erase_block_size) ||
        (parsed.application_start % parsed.erase_block_size) != 0U ||
        parsed.minimum_image_size <
            firmware_update::kManifestOffset +
                firmware_update::kManifestSize + 4U ||
        (parsed.installed_app_size != 0xFFFFFFFFU &&
         (parsed.installed_app_size < parsed.minimum_image_size ||
          parsed.installed_app_size > parsed.application_capacity ||
          (parsed.installed_app_size & (kDataBytes - 1U)) != 0U)) ||
        parsed.can_mode_mask == 0U || parsed.nominal_bitrate_mask == 0U ||
        parsed.checksum_type != firmware_update::kChecksumCrc32 ||
        parsed.protocol_id != firmware_update::kProtocolId ||
        parsed.request_id == parsed.response_id ||
        (parsed.can_id_format == 0U &&
         (parsed.request_id > 0x7FFU || parsed.response_id > 0x7FFU)) ||
        (parsed.can_id_format == 1U &&
         (parsed.request_id > 0x1FFFFFFFU ||
          parsed.response_id > 0x1FFFFFFFU)) ||
        parsed.can_id_format != 0U || parsed.block_bytes != kBlockBytes ||
        !parsed.block_data_id || parsed.block_data_id > 0x7F8U ||
        (parsed.block_data_id & 7U) != 0U ||
        (parsed.request_id & 0x7F8U) == parsed.block_data_id ||
        (parsed.response_id & 0x7F8U) == parsed.block_data_id ||
        parsed.mcu_part.empty() || parsed.mcu_family.empty()) {
        return Fail(L"CAN Upgrade target metadata is incompatible.", error);
    }

    *info = parsed;
    block_data_id_ = parsed.block_data_id;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool Client::EnterBootloader(std::wstring* error) {
    can_host::Frame request;
    can_host::Frame response;
    Ack ack;
    return BuildEnterRequest(&request, error) &&
           Exchange(request, 2U, normal_timeout_ms_, &response, error) &&
           ParseEnterResponse(response, &ack, error) &&
           RequireOk(ack, error);
}

bool Client::GetAppHealth(std::uint32_t* health, std::wstring* error) {
    can_host::Frame request, response;
    DeviceState state;
    const auto page=InfoPage::Health;
    return health && BuildInfoRequest(page,&request,error) &&
        Exchange(request,8U,normal_timeout_ms_,&response,error) &&
        ParseInfoResponse(page,response,&state,health,error) && state==DeviceState::Application;
}

bool Client::VerifyInstalled(Ack* ack, std::wstring* error) {
    can_host::Frame request, response;
    return ack && BuildRequest({static_cast<std::uint8_t>(Command::Verify)},&request,error) &&
        Exchange(request,8U,end_timeout_ms_,&response,error) &&
        ParseAckResponse(Command::Verify,response,ack,error) && RequireOk(*ack,error) && ack->sequence==0U;
}

bool Client::GetDiagnostics(std::uint32_t* flags,std::wstring* error) {
    can_host::Frame request,response;
    DeviceState state;
    return flags && BuildInfoRequest(InfoPage::Diagnostics,&request,error) &&
        Exchange(request,8U,normal_timeout_ms_,&response,error) &&
        ParseInfoResponse(InfoPage::Diagnostics,response,&state,flags,error);
}

bool Client::StageMetadata(const std::vector<std::uint8_t>& image,
    const std::function<bool()>& cancelled, std::wstring* error) {
    firmware_update::ImageInfo parsed;
    if (!firmware_update::ValidateImage(firmware_update::Profile::CanUpgrade,image,&parsed,error)) return false;
    can_host::Frame reset_request, reset_response; Ack reset_ack;
    if (cancelled && cancelled()) return Fail(L"Metadata staging cancelled before reset.",error);
    if (!BuildRequest({static_cast<std::uint8_t>(Command::MetadataReset)},&reset_request,error) ||
        !Exchange(reset_request,8U,normal_timeout_ms_,&reset_response,error) ||
        !ParseAckResponse(Command::MetadataReset,reset_response,&reset_ack,error)) return false;
    if (reset_ack.status==Status::BadState) {
        Ack abort_ack;
        if (!Abort(&abort_ack,error) || !Exchange(reset_request,8U,normal_timeout_ms_,&reset_response,error) ||
            !ParseAckResponse(Command::MetadataReset,reset_response,&reset_ack,error)) return false;
    }
    if (!RequireOk(reset_ack,error) || reset_ack.sequence!=0U || reset_ack.value!=256U)
        return Fail(L"Unable to reset stale metadata session; START blocked.",error);
    const auto* region=image.data()+kManifestOffset;
    for (std::uint16_t offset=0; offset<=256U; offset+=4U) {
        if (cancelled && cancelled()) return Fail(L"Metadata staging cancelled before START / erase.",error);
        can_host::Frame request,response; Ack ack;
        std::vector<std::uint8_t> data(8U,0U);
        const bool seal=offset==256U;
        const Command command=seal?Command::Seal:Command::Metadata;
        data[0]=static_cast<std::uint8_t>(command);
        if (seal) {
            const auto crc=Crc32(region,256U);
            for (unsigned i=0; i<4; ++i) data[4U+i]=static_cast<std::uint8_t>(crc>>(i*8U));
        } else {
            data[1]=static_cast<std::uint8_t>(offset); data[2]=static_cast<std::uint8_t>(offset>>8U);
            std::copy_n(region+offset,4U,data.data()+4U);
        }
        if (!BuildRequest(data,&request,error) || !Exchange(request,8U,normal_timeout_ms_,&response,error) ||
            !ParseAckResponse(command,response,&ack,error) || !RequireOk(ack,error)) return false;
        if (ack.sequence!=(seal?256U:offset+4U) || ack.value!=256U)
            return Fail(L"Metadata staging ACK offset/size mismatch; START blocked.",error);
    }
    return true;
}

bool Client::Start(const ImageInfo& image,
                   Ack* ack,
                   std::wstring* error) {
    can_host::Frame request;
    can_host::Frame response;
    if (image.application_start != application_start_ ||
        image.application_capacity != application_capacity_ ||
        image.erase_block_size != erase_block_size_) {
        return Fail(L"CAN Upgrade image layout does not match target "
                    L"metadata.", error);
    }
    if (!BuildStartRequest(image, &request, error) ||
        !Exchange(request, 8U, normal_timeout_ms_, &response, error) ||
        !ParseAckResponse(image.force_old_version ? Command::ForceStart : Command::Start, response, ack, error) ||
        !RequireOk(*ack, error)) {
        return false;
    }
    if (ack->sequence != 0U || ack->value != application_start_) {
        return Fail(L"F13 Boot START ACK state is invalid.", error);
    }
    return true;
}

bool Client::SendBlock(
    std::uint32_t image_offset,
    const std::vector<std::uint8_t>& data,
    Ack* ack,
    std::wstring* error) {
    if (!client_ || !ack || extended_id_ || !block_data_id_ || data.empty() ||
        data.size() > kBlockBytes || data.size() % kDataBytes ||
        image_offset >= application_capacity_ || data.size() > application_capacity_ - image_offset) {
        return Fail(L"Invalid/unprobed CAN Upgrade block.", error);
    }
    const auto crc = Crc32(data.data(), data.size());
    can_host::Frame begin, commit;
    if (!BuildBlockRequest(Command::BlockBegin, image_offset, crc, &begin, error) ||
        !BuildBlockRequest(Command::BlockCommit, image_offset, crc, &commit, error)) return false;
    begin.id = commit.id = request_id_;
    std::vector<can_host::Frame> frames{begin};
    for (std::size_t offset = 0U; offset < data.size(); offset += 8U) {
        can_host::Frame segment;
        segment.id = block_data_id_ + static_cast<std::uint32_t>(offset / 8U);
        const auto length = (std::min)(std::size_t{8U}, data.size() - offset);
        segment.dlc = static_cast<std::uint8_t>(length);
        segment.data.assign(data.begin() + offset, data.begin() + offset + length);
        frames.push_back(segment);
    }
    frames.push_back(commit);
    can_host::Frame response;
    const std::uint32_t address =
        application_start_ + image_offset;
    const std::uint16_t timeout =
        (address % erase_block_size_) == 0U ?
            erase_timeout_ms_ : normal_timeout_ms_;
    can_host::ReceiveMatch match;
    match.timeout_ms = timeout;
    match.id = response_id_;
    match.id_mask = 0x7FFU;
    match.expected_dlc = 8U;
    match.flags_mask = 0x0FU;
    for (unsigned attempt = 0U; ; ++attempt) {
        host_link::Status completion;
        if (!client_->BurstWaitReceive(frames, match, 0U, &response, error, &completion)) {
            if (completion == host_link::Status::Timeout && attempt < retry_count_) continue;
            return false;
        }
        response.id = kResponseId;
        if (!ParseAckResponse(Command::BlockCommit, response, ack, error)) return false;
        if ((ack->status == Status::BadSequence || ack->status == Status::CrcError) &&
            attempt < retry_count_) continue;
        if (!RequireOk(*ack, error)) return false;
        break;
    }
    if (ack->sequence != static_cast<std::uint16_t>(
                             (image_offset / kBlockBytes) & 0xFFFFU) ||
        ack->value != address + data.size()) {
        return Fail(L"F13 Boot BLOCK ACK state is invalid.", error);
    }
    return true;
}

bool Client::End(std::uint32_t expected_crc32,
                 std::uint32_t expected_byte_count,
                 Ack* ack,
                 std::wstring* error) {
    can_host::Frame request;
    can_host::Frame response;
    if (!BuildSimpleRequest(Command::End, &request, error) ||
        !Exchange(request, 8U, end_timeout_ms_, &response, error) ||
        !ParseAckResponse(Command::End, response, ack, error) ||
        !RequireOk(*ack, error)) {
        return false;
    }
    if (ack->sequence != static_cast<std::uint16_t>(
                             (expected_byte_count / kDataBytes) & 0xFFFFU) ||
        ack->value != expected_crc32) {
        return Fail(L"F13 Boot END ACK state or CRC32 is invalid.", error);
    }
    return true;
}

bool Client::Abort(Ack* ack, std::wstring* error) {
    can_host::Frame request;
    can_host::Frame response;
    return BuildSimpleRequest(Command::Abort, &request, error) &&
           Exchange(request, 8U, normal_timeout_ms_, &response, error) &&
           ParseAckResponse(Command::Abort, response, ack, error) &&
           RequireOk(*ack, error);
}

bool Client::Exchange(const can_host::Frame& request,
                      std::uint8_t expected_dlc,
                      std::uint16_t timeout_ms,
                      can_host::Frame* response,
                      std::wstring* error) {
    if (client_ == nullptr) {
        return Fail(L"F13 Boot CAN client is not configured.", error);
    }

    can_host::Frame actual_request = request;
    actual_request.id = request_id_;
    actual_request.extended = extended_id_;
    can_host::ReceiveMatch match;
    match.timeout_ms = timeout_ms;
    match.retry_count = retry_count_;
    match.expected_dlc = expected_dlc;
    match.id = response_id_;
    match.id_mask = extended_id_ ? 0x1FFFFFFFU : 0x7FFU;
    match.flags_value = extended_id_ ? 0x01U : 0U;
    match.flags_mask = 0x07U;
    if (!client_->TransmitWaitReceive(
            actual_request, match, response, error)) {
        return false;
    }
    response->id = kResponseId;
    response->extended = false;
    return true;
}

bool Client::RequireOk(const Ack& ack, std::wstring* error) {
    if (ack.status == Status::Ok) {
        return true;
    }

    std::wstringstream stream;
    stream << L"F13 Boot command 0x" << std::hex
           << static_cast<unsigned>(ack.command) << L" returned "
           << StatusText(ack.status) << L"; sequence=" << std::dec
           << ack.sequence << L", value=0x" << std::hex << ack.value << L".";
    if (error != nullptr) {
        *error = stream.str();
    }
    return false;
}

} // namespace firmware_upgrade::f13_boot
