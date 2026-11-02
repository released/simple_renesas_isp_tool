#include "image_analysis.h"
#include <windows.h>
#include <algorithm>
#include <utility>

namespace firmware_upgrade::image_analysis {
namespace {
struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
bool Fail(std::wstring* error, const std::wstring& reason) {
    if (error) *error = reason;
    return false;
}
}
bool Load(const std::wstring& path, const std::function<bool()>& canceled,
          Result* result, std::wstring* error) {
    if (!result) return Fail(error, L"BIN analysis output is missing.");
    const auto stop = [&]() { return canceled && canceled(); };
    if (stop()) return Fail(error, L"BIN load canceled.");
    File file;
    // Excluding concurrent writers makes this an exact, immutable input snapshot.
    file.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE)
        return Fail(error, L"BIN open failed (Windows " + std::to_wstring(GetLastError()) + L"): " + path);
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file.handle, &size)) return Fail(error, L"BIN size query failed.");
    if (size.QuadPart <= 0) return Fail(error, L"Firmware image is empty.");
    if (static_cast<std::uint64_t>(size.QuadPart) > kMaximumFileBytes)
        return Fail(error, L"BIN exceeds the supported 16 MiB image limit. Check the selected file.");
    Result loaded;
    loaded.path = path;
    loaded.bytes.resize(static_cast<std::size_t>(size.QuadPart));
    for (std::size_t offset = 0U; offset < loaded.bytes.size();) {
        if (stop()) return Fail(error, L"BIN load canceled.");
        const auto count = static_cast<DWORD>((std::min)(std::size_t{65536U}, loaded.bytes.size() - offset));
        DWORD read = 0U;
        if (!ReadFile(file.handle, loaded.bytes.data() + offset, count, &read, nullptr) || !read)
            return Fail(error, L"BIN read failed or was incomplete (Windows " + std::to_wstring(GetLastError()) + L").");
        offset += read;
    }
    if (stop()) return Fail(error, L"BIN load canceled.");
    const bool host = loaded.bytes.size() >= 260U &&
        image_policy::Read32(loaded.bytes.data() + 256U) == BU_IMAGE_MAGIC;
    if (host) {
        loaded.bridge = bridge_update::Inspect(loaded.bytes);
        loaded.device.image_size = loaded.bytes.size();
        loaded.device.checksum_available = loaded.bytes.size() >= 4U;
        loaded.device.calculated_crc32 = loaded.bridge.calculated_crc;
        loaded.device.stored_crc32 = loaded.bridge.stored_crc;
        loaded.device.checksum_matches = loaded.bridge.crc_matches;
        loaded.device.metadata_error = L"Loaded BIN is a Host Bridge image; use Misc.";
    } else {
        loaded.device = firmware_update::InspectImage(loaded.bytes);
        // Do not calculate an unrelated reference checksum for a known device image.
        if (!loaded.device.metadata_available) loaded.reference = g16_i2c::InspectImage(loaded.bytes);
    }
    if (stop()) return Fail(error, L"BIN analysis canceled.");
    *result = std::move(loaded);
    if (error) error->clear();
    return true;
}
} // namespace firmware_upgrade::image_analysis
