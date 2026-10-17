#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "image_policy.h"

namespace firmware_upgrade::firmware_update {

/*
 * The manifest is the portable target contract.  The PC must not infer flash
 * layout or CAN settings from an MCU name.  image_size is the exact BIN length
 * including its final four-byte CRC32; application_capacity is only the upper
 * bound reported by this firmware build.  Device discovery must report the
 * same identity/layout/CAN fields before programming can start.
 */

enum class Profile {
    None,
    CanUpgrade,
};

struct ImageInfo {
    image_policy::Policy policy;
    Profile profile = Profile::None;
    std::wstring profile_name;
    std::wstring mcu_family;
    std::wstring mcu_part;
    std::uint8_t format = 0U;
    std::uint8_t manifest_length = 0U;
    std::uint16_t flags = 0U;
    std::uint16_t protocol_id = 0U;
    std::uint8_t protocol_version = 0U;
    std::uint8_t checksum_type = 0U;
    std::uint16_t target_id = 0U;
    std::uint8_t can_mode_mask = 0U;
    std::uint8_t can_id_format = 0U;
    std::uint32_t version = 0U;
    std::uint32_t build = 0U;
    std::uint32_t image_size = 0U;
    std::uint32_t application_start = 0U;
    std::uint32_t application_end = 0U;
    std::uint32_t application_capacity = 0U;
    std::uint32_t code_flash_size = 0U;
    std::uint32_t boot_reserved_size = 0U;
    std::uint32_t erase_block_size = 0U;
    std::uint32_t nominal_bitrate_mask = 0U;
    std::uint32_t data_bitrate_mask = 0U;
    std::uint32_t request_id = 0U;
    std::uint32_t response_id = 0U;
    std::uint32_t minimum_image_size = 0U;
    std::uint32_t calculated_crc32 = 0U;
    std::uint32_t stored_crc32 = 0U;
};

struct ImageInspection {
    std::uint64_t image_size = 0U;
    bool checksum_available = false;
    std::uint32_t calculated_crc32 = 0U;
    std::uint32_t stored_crc32 = 0U;
    bool checksum_matches = false;
    bool metadata_available = false;
    bool metadata_valid = false;
    ImageInfo metadata;
    std::wstring metadata_error;
};

constexpr std::uint32_t kManifestOffset = 0x100U;
constexpr std::uint32_t kManifestMagic = 0x004E4143U; // ASCII CAN + NUL.
constexpr std::uint8_t kImageFormatVersion = 6U;
constexpr std::uint8_t kManifestSize = IP_CORE_SIZE;
constexpr std::size_t kMetadataRegionSize = 256U;
constexpr std::uint16_t kProtocolId = 1U;
constexpr std::uint8_t kProtocolVersion = 5U;
constexpr std::uint8_t kChecksumCrc32 = 1U;

Profile FromDisplayName(const std::wstring& display_name);
const wchar_t* DisplayName(Profile profile);
std::uint32_t Crc32(const std::uint8_t* data, std::size_t length);
ImageInspection InspectImage(const std::vector<std::uint8_t>& image);
bool ValidateImage(Profile profile,
                   const std::vector<std::uint8_t>& image,
                   ImageInfo* info,
                   std::wstring* error);

} // namespace firmware_upgrade::firmware_update
