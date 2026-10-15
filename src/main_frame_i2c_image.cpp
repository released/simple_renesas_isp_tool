#include "main_frame.h"
#include <iomanip>
#include <sstream>

namespace {
std::wstring Hex(std::uint32_t value, unsigned width = 8U) {
    std::wstringstream text;
    text << L"0x" << std::hex << std::uppercase << std::setfill(L'0')
         << std::setw(static_cast<int>(width)) << value;
    return text.str();
}
}
void CMainFrame::UpdateI2cImageFields(bool target_current) {
    firmware_field_labels_[kFirmwareFieldCalculatedCrc].SetWindowTextW(L"Calculated CRC16");
    firmware_field_labels_[kFirmwareFieldStoredCrc].SetWindowTextW(L"Stored CRC16");
    for (auto& value : firmware_field_values_) value.SetWindowTextW(L"N/A");
    if (!binary_data_.empty()) {
        firmware_field_values_[kFirmwareFieldBinSize].SetWindowTextW(
            (std::to_wstring(binary_data_.size()) + L" bytes").c_str());
        firmware_field_values_[kFirmwareFieldMetadataStatus].SetWindowTextW(L"RAW BIN - no manifest");
    }
    if (binary_i2c_inspection_.available) {
        firmware_field_values_[kFirmwareFieldCalculatedCrc].SetWindowTextW(
            Hex(binary_i2c_inspection_.calculated_crc16, 4U).c_str());
        firmware_field_values_[kFirmwareFieldStoredCrc].SetWindowTextW(
            Hex(binary_i2c_inspection_.stored_crc16, 4U).c_str());
        firmware_field_values_[kFirmwareFieldChecksumResult].SetWindowTextW(
            binary_i2c_inspection_.matches ? L"MATCH (G16 reference)" : L"MISMATCH");
    }
    std::wstring text = L"G16 BIN has no embedded MCU/version/start metadata. CRC16/XMODEM, init0, range [0,size-5], stored LE16 at size-4. MATCH alone does not establish identity or safe update.";
    if (target_current) {
        const auto& info = session_state_.i2c_metadata;
        text = L"LIVE G16 reference label: " + info.label + L"; boot " +
            std::to_wstring(info.boot_major) + L"." + std::to_wstring(info.boot_minor) +
            L", app " + std::to_wstring(info.app_major) + L"." +
            std::to_wstring(info.app_minor) + L" (reported constants, not authenticated).";
        for (const auto& area : info.areas) {
            text += L" Area " + std::to_wstring(area.kind) + L": " + Hex(area.start) +
                L".." + Hex(area.end) + L"; erase/write/read=" +
                std::to_wstring(area.erase_unit) + L"/" + std::to_wstring(area.write_unit) +
                L"/" + std::to_wstring(area.read_unit) + L" B.";
        }
        std::wstring fit_error;
        if (!binary_data_.empty() && !firmware_upgrade::g16_i2c::ImageFitsDevice(
            info, binary_i2c_inspection_, binary_data_.size(), &fit_error))
            text += L" BIN/geometry check: " + fit_error;
        // Live target fields never overwrite BIN metadata: raw files lack it.
    }
    target_metadata_label_.SetWindowTextW(text.c_str());
}
