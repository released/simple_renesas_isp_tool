#include "g16_i2c_reference.h"

namespace firmware_upgrade::g16_i2c {
std::uint16_t ImageCrc16(const std::uint8_t* data, std::size_t length) {
    std::uint16_t crc = 0U;
    for (std::size_t i = 0U; i < length; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i] << 8);
        for (unsigned bit = 0U; bit < 8U; ++bit)
            crc = static_cast<std::uint16_t>((crc & 0x8000U) != 0U ?
                (crc << 1) ^ 0x1021U : crc << 1);
    }
    return crc;
}
ImageInspection InspectImage(const std::vector<std::uint8_t>& image) {
    ImageInspection result;
    if (image.size() < 8U || image.size() > 0x10000U) return result;
    // G16 sample linker reserves four bytes, but stores a two-byte LE CRC16.
    // BIN has no MCU/version/start manifest. Do not infer these from the CRC.
    result.available = true;
    result.calculated_crc16 = ImageCrc16(image.data(), image.size() - 4U);
    result.stored_crc16 = static_cast<std::uint16_t>(image[image.size() - 4U] |
        (image[image.size() - 3U] << 8));
    result.matches = result.calculated_crc16 == result.stored_crc16;
    return result;
}
bool ImageFitsDevice(const DeviceInfo& info, const ImageInspection& inspection,
                     std::size_t image_size, std::wstring* error) {
    const Area* code = nullptr;
    for (const auto& area : info.areas) if (area.kind == 0U) {
        if (code != nullptr) {
            if (error != nullptr) *error = L"Select an unambiguous G16 code area; multiple code areas are unsupported.";
            return false;
        }
        code = &area;
    }
    if (info.label != L"RL78_G16_FPB" || code == nullptr || code->start == 0U ||
        code->end < code->start || code->end > 0xFFFFU ||
        !inspection.available || !inspection.matches ||
        image_size != static_cast<std::size_t>(code->end - code->start) + 1U ||
        code->write_unit == 0U || code->write_unit > 256U ||
        image_size % code->write_unit != 0U) {
        if (error != nullptr) *error = L"G16 reference requires its confirmed identity, full advertised code-area BIN and valid CRC16; raw BIN has no embedded identity.";
        return false;
    }
    return true;
}
std::wstring ProgrammingBlocker() {
    return L"G16 reference slave: READ/0x15 and real ID authentication are not implemented; boot/app CRC entry and recovery gates need correction. Flash erase/write/app jump are blocked.";
}
} // namespace firmware_upgrade::g16_i2c
