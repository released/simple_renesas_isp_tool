#include "g16_i2c_reference.h"

namespace firmware_upgrade::g16_i2c {
namespace {
bool Fail(std::wstring* error, const wchar_t* text) {
    if (error != nullptr) *error = text;
    return false;
}
std::uint32_t Be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
        (static_cast<std::uint32_t>(p[1]) << 16) |
        (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}
}
bool ParseSignature(const std::vector<std::uint8_t>& packet, DeviceInfo* info,
                    std::uint8_t* area_count, std::wstring* error) {
    if (info == nullptr || area_count == nullptr || packet.size() != 25U ||
        !ValidatePacket(packet, 0x3AU) || packet[4] == 0U || packet[4] > 8U)
        return Fail(error, L"Invalid G16 signature/status/checksum/area count.");
    DeviceInfo result;
    bool padding = false;
    for (std::size_t i = 6U; i < 19U; ++i) {
        if (packet[i] == 0U) { padding = true; continue; }
        if (padding || packet[i] < 32U || packet[i] > 126U)
            return Fail(error, L"Invalid G16 identity label.");
        result.label += static_cast<wchar_t>(packet[i]);
    }
    if (result.label.empty()) return Fail(error, L"G16 signature has no identity label.");
    result.legacy_type = packet[5];
    result.boot_major = packet[19]; result.boot_minor = packet[20];
    result.app_major = packet[21]; result.app_minor = packet[22];
    *area_count = packet[4]; *info = std::move(result);
    return true;
}
bool ParseArea(const std::vector<std::uint8_t>& packet, Area* area,
               std::wstring* error) {
    if (area == nullptr || packet.size() != 27U || !ValidatePacket(packet, 0x3BU) ||
        packet[4] > 2U) return Fail(error, L"Invalid G16 area packet/status/checksum.");
    Area result;
    result.kind = packet[4]; result.start = Be32(&packet[5]);
    result.end = Be32(&packet[9]); result.erase_unit = Be32(&packet[13]);
    result.write_unit = Be32(&packet[17]); result.read_unit = Be32(&packet[21]);
    if (result.end < result.start || result.end > 0xFFFFU ||
        result.erase_unit == 0U || result.write_unit == 0U || result.write_unit > 256U ||
        result.read_unit == 0U || result.read_unit > 256U ||
        result.start % result.erase_unit != 0U ||
        (result.end + 1U) % result.erase_unit != 0U)
        return Fail(error, L"G16 flash geometry exceeds the reference's safe 16-bit/alignment/unit bounds.");
    *area = result; return true;
}
bool Discover(i2c_host::Client& client, std::uint8_t address, DeviceInfo* info,
              std::wstring* error) {
    if (info == nullptr) return Fail(error, L"G16 discovery output is null.");
    if (!Probe(client, address, error)) return false;
    i2c_host::Transaction transaction;
    transaction.address = address; transaction.repeated_start = true;
    transaction.transmit = EncodePacket(1U, 0x3AU, {});
    transaction.receive_length = 25U;
    std::vector<std::uint8_t> packet;
    DeviceInfo result; std::uint8_t count = 0U;
    if (!client.Transfer(transaction, &packet, error) ||
        !ParseSignature(packet, &result, &count, error)) return false;
    for (std::uint8_t i = 0U; i < count; ++i) {
        transaction.transmit = EncodePacket(1U, 0x3BU, {i});
        transaction.receive_length = 27U;
        Area area;
        if (!client.Transfer(transaction, &packet, error) ||
            !ParseArea(packet, &area, error)) return false;
        for (const auto& previous : result.areas)
            if (area.start <= previous.end && previous.start <= area.end)
                return Fail(error, L"Overlapping G16 flash areas are unsafe.");
        result.areas.push_back(area);
    }
    *info = std::move(result); return true;
}
} // namespace firmware_upgrade::g16_i2c
