#include "../src/core/image_analysis.h"
#include "../src/core/text_file.h"
#include "../src/core/can_image_contract.h"
#include "../src/core/update_evidence.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int TestSerialPortIo();
int TestToolExperience();
int TestTextFileFaults();
namespace {
int failures = 0;
unsigned cases = 0;
void Check(bool ok, const char* name) {
    ++cases;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
void File(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Check(out.good(), "owned fixture written");
}
firmware_upgrade::f13_boot::TargetInfo Target(const firmware_upgrade::firmware_update::ImageInfo& i) {
    firmware_upgrade::f13_boot::TargetInfo t;
    t.target_id = i.target_id; t.protocol_id = static_cast<std::uint8_t>(i.protocol_id); t.protocol_version = i.protocol_version;
    t.image_format = i.format; t.checksum_type = i.checksum_type; t.application_start = i.application_start;
    t.application_capacity = i.application_capacity; t.code_flash_size = i.code_flash_size;
    t.boot_reserved_size = i.boot_reserved_size; t.erase_block_size = i.erase_block_size;
    t.minimum_image_size = i.minimum_image_size; t.can_mode_mask = i.can_mode_mask; t.can_id_format = i.can_id_format;
    t.nominal_bitrate_mask = i.nominal_bitrate_mask; t.data_bitrate_mask = i.data_bitrate_mask;
    t.request_id = i.request_id; t.response_id = i.response_id; t.mcu_part = i.mcu_part; t.mcu_family = i.mcu_family;
    t.policy = i.policy; t.boot_version = i.policy.minimum_boot_version;
    t.app_version = i.version; t.app_build = i.build; t.installed_app_size = i.image_size;
    return t;
}
}
int wmain(int argc, wchar_t** argv) {
    using namespace firmware_upgrade;
    if (argc != 2) return 2;
    Check(TestSerialPortIo() == 0, "actual Win32 transport model with inherited DCB flags");
    Check(TestToolExperience() == 0, "report/guidance/preflight regressions");
    Check(TestTextFileFaults() == 0, "actual text helper short-write/flush/close faults");
    std::wstring error;
    std::string utf8;
    Check(text_file::Utf8(L"abc\u6e2c\u8a66", &utf8, &error) && utf8 == "abc\xE6\xB8\xAC\xE8\xA9\xA6", "exact UTF-8 bytes/count without NUL");
    Check(text_file::Utf8(L"", &utf8, &error) && utf8.empty(), "empty UTF-8 output");
    Check(!text_file::Utf8(std::wstring(1U, 0xD800U), &utf8, &error), "invalid UTF-16 is reported");
    const auto directory = std::filesystem::current_path();
    FILE* owner = nullptr;
    const auto exclusive = (directory / L"exclusive.log").wstring();
    Check(text_file::OpenExclusive(exclusive, &owner, &error), "exclusive first creation");
    Check(text_file::Write(owner, "preserve", &error) && text_file::Close(&owner, &error), "checked write/flush/close");
    Check(!text_file::OpenExclusive(exclusive, &owner, &error) && !owner, "same-name log cannot overwrite existing bytes");
    std::ifstream prior(exclusive, std::ios::binary);
    Check(std::string(std::istreambuf_iterator<char>(prior), {}) == "preserve", "original evidence preserved");
    prior.close();
    std::wstring name1, name2;
    Check(text_file::OpenLog(directory.wstring(), &name1, &owner, &error) && text_file::Close(&owner, &error), "first unique log");
    Check(text_file::OpenLog(directory.wstring(), &name2, &owner, &error) && text_file::Close(&owner, &error) && name1 != name2, "rapid second log has a different name");
    image_analysis::Result input;
    Check(image_analysis::Load(argv[1], {}, &input, &error) && input.device.metadata_valid && input.device.checksum_matches, "actual golden BIN bounded load and validation");
    const auto inspection = input.device;
    firmware_update::ImageInfo metadata;
    Check(firmware_update::ValidateInspection(firmware_update::Profile::CanUpgrade, inspection, &metadata, &error), "offline cached validation");
    auto modified = input.bytes;
    modified.back() ^= 1U;
    Check(!firmware_update::ValidateImage(firmware_update::Profile::CanUpgrade, modified, &metadata, &error), "write-time reinspection rejects changed bytes despite earlier valid cache");
    auto t = Target(inspection.metadata);
    Check(can_contract::Matches(inspection.metadata, t), "matching contract");
    const auto verify = [&](const firmware_update::ImageInfo& i) {
        const auto rows = evidence::Compare(i, t);
        const bool blocked = std::any_of(rows.begin(), rows.end(), [](const evidence::Difference& d) { return d.result == L"MISMATCH"; });
        Check(can_contract::Matches(i, t) == !blocked, "shared display/write-time contract decision");
    };
    using Image = firmware_update::ImageInfo;
    for (auto member : {&Image::application_start, &Image::application_capacity, &Image::code_flash_size,
         &Image::boot_reserved_size, &Image::erase_block_size, &Image::minimum_image_size,
         &Image::nominal_bitrate_mask, &Image::data_bitrate_mask, &Image::request_id, &Image::response_id}) {
        auto i = inspection.metadata; ++(i.*member); verify(i);
        Check(!can_contract::Matches(i, t, &error) && !error.empty(), "numeric contract mismatch reports a field");
    }
    for (unsigned at = 0; at < 9U; ++at) {
        auto i = inspection.metadata;
        switch (at) {
        case 0: ++i.target_id; break;
        case 1: ++i.protocol_id; break;
        case 2: ++i.protocol_version; break;
        case 3: ++i.format; break;
        case 4: ++i.checksum_type; break;
        case 5: ++i.can_mode_mask; break;
        case 6: ++i.can_id_format; break;
        case 7: i.mcu_part += L"X"; break;
        default: i.mcu_family += L"X"; break;
        }
        verify(i); Check(!can_contract::Matches(i, t), "identity/format mismatch blocked");
    }
    for (unsigned at = 0; at < 4U; ++at) {
        auto i = inspection.metadata;
        if (at == 0) i.policy.role = IP_ROLE_BRIDGE_APP;
        else if (at == 1) i.policy.transports = IP_UART;
        else if (at == 2) ++i.policy.irq_abi;
        else ++i.policy.minimum_boot_version;
        verify(i); Check(!can_contract::Matches(i, t), "policy/ABI/minimum Boot mismatch blocked");
    }
    auto release = inspection.metadata; ++release.version; ++release.build; ++release.image_size; release.policy.version_control ^= 1U;
    verify(release); Check(can_contract::Matches(release, t), "release changes stay informational for ordinary matching");
    image_analysis::Result failed;
    Check(!image_analysis::Load(L"not-present.bin", {}, &failed, &error), "missing file reported");
    File(directory / L"empty.bin", {});
    Check(!image_analysis::Load((directory / L"empty.bin").wstring(), {}, &failed, &error), "empty file rejected before analysis");
    Check(!image_analysis::Load(argv[1], [] { return true; }, &failed, &error) && error.find(L"canceled") != std::wstring::npos, "cancel before read");
    unsigned polls = 0U;
    Check(!image_analysis::Load(argv[1], [&] { return ++polls > 1U; }, &failed, &error), "cancel during bounded read");
    const auto oversized = directory / L"oversized.bin";
    File(oversized, {}); std::filesystem::resize_file(oversized, image_analysis::kMaximumFileBytes + 1U);
    Check(!image_analysis::Load(oversized.wstring(), {}, &failed, &error) && failed.bytes.empty() && error.find(L"16 MiB") != std::wstring::npos, "oversized file rejected before allocation");
    const auto large = directory / L"large.bin";
    File(large, std::vector<std::uint8_t>(16U * 1024U * 1024U, 0xFFU));
    const auto start = std::chrono::steady_clock::now();
    Check(image_analysis::Load(large.wstring(), {}, &failed, &error) && failed.bytes.size() == image_analysis::kMaximumFileBytes, "maximum bounded file inspected");
    std::cout << "16MiB worker read/inspection ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << '\n';
    std::cout << "Review core cases=" << cases << ", failures=" << failures << '\n';
    return failures ? 1 : 0;
}
