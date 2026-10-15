#include "firmware_update_profile.h"

namespace firmware_upgrade::firmware_update {
namespace {

constexpr std::uint16_t kRequiredFlags = 0x0003U;
constexpr std::size_t kPartOffset = 0x40U;
constexpr std::size_t kPartLength = 16U;
constexpr std::size_t kFamilyOffset = 0x50U;
constexpr std::size_t kFamilyLength = 12U;

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

bool Fail(const std::wstring& message, std::wstring* error) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool IsPowerOfTwo(std::uint32_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool ReadAsciiField(const std::uint8_t* data,
                    std::size_t length,
                    std::wstring* output) {
    output->clear();
    bool found_terminator = false;
    for (std::size_t index = 0U; index < length; ++index) {
        const std::uint8_t value = data[index];
        if (value == 0U) {
            found_terminator = true;
            continue;
        }
        if (found_terminator || value < 0x20U || value > 0x7EU) {
            return false;
        }
        output->push_back(static_cast<wchar_t>(value));
    }
    return !output->empty();
}

bool ParseMetadata(const std::vector<std::uint8_t>& image,
                   ImageInfo* info,
                   std::wstring* error) {
    if (info == nullptr) {
        return Fail(L"Firmware metadata output is null.", error);
    }
    if (image.size() < kManifestOffset + kMetadataRegionSize + 4U) {
        return Fail(L"Firmware image is too small to contain CAN Upgrade "
                    L"metadata and a CRC32 trailer.", error);
    }

    const std::uint8_t* manifest = &image[kManifestOffset];
    if (ReadLe32(manifest) != kManifestMagic) {
        return Fail(L"CAN Upgrade image metadata magic is invalid.", error);
    }

    ImageInfo parsed;
    parsed.profile = Profile::CanUpgrade;
    parsed.profile_name = DisplayName(parsed.profile);
    parsed.format = manifest[4];
    parsed.manifest_length = manifest[5];
    parsed.flags = ReadLe16(&manifest[6]);
    parsed.protocol_id = ReadLe16(&manifest[8]);
    parsed.protocol_version = manifest[10];
    parsed.checksum_type = manifest[11];
    parsed.target_id = ReadLe16(&manifest[12]);
    parsed.can_mode_mask = manifest[14];
    parsed.can_id_format = manifest[15];
    parsed.version = ReadLe32(&manifest[16]);
    parsed.build = ReadLe32(&manifest[20]);
    parsed.image_size = ReadLe32(&manifest[24]);
    parsed.application_start = ReadLe32(&manifest[28]);
    parsed.application_capacity = ReadLe32(&manifest[32]);
    parsed.code_flash_size = ReadLe32(&manifest[36]);
    parsed.boot_reserved_size = ReadLe32(&manifest[40]);
    parsed.erase_block_size = ReadLe32(&manifest[44]);
    parsed.nominal_bitrate_mask = ReadLe32(&manifest[48]);
    parsed.data_bitrate_mask = ReadLe32(&manifest[52]);
    parsed.request_id = ReadLe32(&manifest[56]);
    parsed.response_id = ReadLe32(&manifest[60]);
    parsed.minimum_image_size = ReadLe32(&manifest[92]);
    if (!image_policy::Parse(manifest, &parsed.policy) || parsed.policy.role!=IP_ROLE_DEVICE_APP) {
        return Fail(L"Image update policy is missing, malformed or unsupported.", error);
    }

    if (parsed.format != kImageFormatVersion ||
        parsed.manifest_length != kManifestSize ||
        parsed.flags != kRequiredFlags ||
        parsed.protocol_id != kProtocolId ||
        parsed.protocol_version != kProtocolVersion ||
        parsed.checksum_type != kChecksumCrc32) {
        return Fail(L"CAN Upgrade image metadata format, protocol, or "
                    L"checksum contract is incompatible.", error);
    }
    if (parsed.target_id == 0U || parsed.version == 0xFFFFFFFFU ||
        parsed.build == 0xFFFFFFFFU) {
        return Fail(L"Firmware target, version, or build metadata is not "
                    L"initialized.", error);
    }
    if (!ReadAsciiField(&manifest[kPartOffset], kPartLength,
                        &parsed.mcu_part) ||
        !ReadAsciiField(&manifest[kFamilyOffset], kFamilyLength,
                        &parsed.mcu_family)) {
        return Fail(L"Firmware MCU family or part metadata is invalid.",
                    error);
    }
    if (parsed.image_size != image.size() ||
        parsed.minimum_image_size < kManifestOffset + kMetadataRegionSize + 4U ||
        parsed.image_size < parsed.minimum_image_size ||
        (parsed.image_size & 3U) != 0U) {
        return Fail(L"Firmware image length does not match its metadata.",
                    error);
    }
    if (ReadLe32(image.data()) == 0xFFFFFFFFU) {
        return Fail(L"Firmware application entry is erased.", error);
    }
    for (std::size_t offset=kManifestSize; offset<kMetadataRegionSize; ++offset) {
        if (manifest[offset]!=0xFFU)
            return Fail(L"Reserved metadata extension bytes must be 0xFF for this image format.",error);
    }
    if (parsed.image_size > 0x01000000U ||
        parsed.application_capacity < parsed.image_size ||
        parsed.application_start >= parsed.code_flash_size ||
        static_cast<std::uint64_t>(parsed.application_start) +
                parsed.application_capacity >
            parsed.code_flash_size ||
        parsed.boot_reserved_size > parsed.application_start ||
        !IsPowerOfTwo(parsed.erase_block_size) ||
        (parsed.application_start % parsed.erase_block_size) != 0U ||
        parsed.can_mode_mask == 0U ||
        (parsed.can_mode_mask & ~0x03U) != 0U ||
        parsed.nominal_bitrate_mask == 0U ||
        ((parsed.can_mode_mask & 0x02U) != 0U &&
         parsed.data_bitrate_mask == 0U)) {
        return Fail(L"Firmware flash-layout or CAN bitrate metadata is "
                    L"invalid.", error);
    }
    if (parsed.can_id_format == 0U) {
        if (parsed.request_id > 0x7FFU || parsed.response_id > 0x7FFU) {
            return Fail(L"Firmware standard CAN identifier metadata is out "
                        L"of range.", error);
        }
    } else if (parsed.can_id_format == 1U) {
        if (parsed.request_id > 0x1FFFFFFFU ||
            parsed.response_id > 0x1FFFFFFFU) {
            return Fail(L"Firmware extended CAN identifier metadata is out "
                        L"of range.", error);
        }
    } else {
        return Fail(L"Firmware CAN identifier format is invalid.", error);
    }
    if (parsed.request_id == parsed.response_id) {
        return Fail(L"Firmware request and response CAN identifiers must "
                    L"differ.", error);
    }

    parsed.application_end = parsed.application_start +
                             parsed.image_size - 1U;
    const std::size_t crc_offset = image.size() - 4U;
    parsed.stored_crc32 = ReadLe32(&image[crc_offset]);
    parsed.calculated_crc32 = Crc32(image.data(), crc_offset);
    *info = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace

Profile FromDisplayName(const std::wstring& display_name) {
    if (display_name == L"CAN Upgrade") {
        return Profile::CanUpgrade;
    }
    return Profile::None;
}

const wchar_t* DisplayName(Profile profile) {
    switch (profile) {
        case Profile::CanUpgrade:
            return L"CAN Upgrade";
        case Profile::None:
        default:
            return L"Unsupported firmware-update protocol";
    }
}

std::uint32_t Crc32(const std::uint8_t* data, std::size_t length) {
    std::uint32_t crc = 0xFFFFFFFFU;
    if (data == nullptr && length != 0U) {
        return 0U;
    }
    for (std::size_t index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 1U) != 0U ?
                (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

ImageInspection InspectImage(const std::vector<std::uint8_t>& image) {
    ImageInspection result;
    result.image_size = static_cast<std::uint64_t>(image.size());
    if (image.size() >= 4U) {
        const std::size_t crc_offset = image.size() - 4U;
        result.checksum_available = true;
        result.stored_crc32 = ReadLe32(&image[crc_offset]);
        result.calculated_crc32 = Crc32(image.data(), crc_offset);
        result.checksum_matches =
            result.calculated_crc32 == result.stored_crc32;
    }
    result.metadata_available =
        image.size() >= kManifestOffset + 4U &&
        ReadLe32(&image[kManifestOffset]) == kManifestMagic;
    result.metadata_valid = ParseMetadata(
        image, &result.metadata, &result.metadata_error);
    return result;
}

bool ValidateImage(Profile profile,
                   const std::vector<std::uint8_t>& image,
                   ImageInfo* info,
                   std::wstring* error) {
    if (profile != Profile::CanUpgrade) {
        return Fail(L"The selected target protocol does not support firmware "
                    L"updates.", error);
    }
    ImageInfo parsed;
    if (!ParseMetadata(image, &parsed, error)) {
        return false;
    }
    if (parsed.stored_crc32 != parsed.calculated_crc32) {
        return Fail(L"Firmware CRC32 trailer does not match the calculated "
                    L"CRC32.", error);
    }
    if (!image_policy::Allows(parsed.policy, IP_ROLE_DEVICE_APP, IP_CAN, error)) return false;
    if (info == nullptr) {
        return Fail(L"Firmware image information output is null.", error);
    }
    *info = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace firmware_upgrade::firmware_update
