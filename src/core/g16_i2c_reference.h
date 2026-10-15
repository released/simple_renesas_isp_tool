#pragma once
#include "i2c_host_client.h"

namespace firmware_upgrade::g16_i2c {
// G16 sample packets use big-endian length fields and additive checksum,
// not CAN Upgrade metadata/CRC32. The supplied slave lacks real ID auth,
// READ and safe boot CRC gating: this profile intentionally cannot program.
struct Area {
    std::uint8_t kind = 0U;
    std::uint32_t start = 0U;
    std::uint32_t end = 0U;
    std::uint32_t erase_unit = 0U;
    std::uint32_t write_unit = 0U;
    std::uint32_t read_unit = 0U;
};
struct DeviceInfo {
    std::wstring label;
    std::uint8_t legacy_type = 0U;
    std::uint8_t boot_major = 0U, boot_minor = 0U;
    std::uint8_t app_major = 0U, app_minor = 0U;
    std::vector<Area> areas;
};
struct ImageInspection {
    bool available = false;
    bool matches = false;
    std::uint16_t calculated_crc16 = 0U;
    std::uint16_t stored_crc16 = 0U;
};
std::vector<std::uint8_t> EncodePacket(std::uint8_t start, std::uint8_t command,
                                     const std::vector<std::uint8_t>& data);
bool ValidatePacket(const std::vector<std::uint8_t>& data,
                    std::uint8_t command);
bool Probe(i2c_host::Client& client, std::uint8_t address, std::wstring* error);
bool ParseSignature(const std::vector<std::uint8_t>& packet, DeviceInfo* info,
                    std::uint8_t* area_count, std::wstring* error);
bool ParseArea(const std::vector<std::uint8_t>& packet, Area* area,
               std::wstring* error);
bool Discover(i2c_host::Client& client, std::uint8_t address, DeviceInfo* info,
              std::wstring* error);
std::uint16_t ImageCrc16(const std::uint8_t* data, std::size_t length);
ImageInspection InspectImage(const std::vector<std::uint8_t>& image);
bool ImageFitsDevice(const DeviceInfo& info, const ImageInspection& inspection,
                     std::size_t image_size, std::wstring* error);
std::wstring ProgrammingBlocker();
} // namespace firmware_upgrade::g16_i2c
