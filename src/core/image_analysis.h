#pragma once
#include "firmware_update_profile.h"
#include "bridge_update_profile.h"
#include "g16_i2c_reference.h"
#include <functional>

namespace firmware_upgrade::image_analysis {
constexpr std::uint64_t kMaximumFileBytes = 0x01000000U;
struct Result {
    std::wstring path;
    bool restored = false;
    std::vector<std::uint8_t> bytes;
    firmware_update::ImageInspection device;
    bridge_update::Image bridge;
    g16_i2c::ImageInspection reference;
};
// Worker-only file I/O and inspection. No COM operation or UI control access.
bool Load(const std::wstring& path, const std::function<bool()>& canceled,
          Result* result, std::wstring* error);
} // namespace firmware_upgrade::image_analysis
