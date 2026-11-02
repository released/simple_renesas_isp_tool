#include "../src/core/firmware_update_profile.h"
#include "../src/core/image_vectors.h"
#include "../src/core/f13_boot_update.h"
#include "../src/ui/report_status.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
unsigned cases = 0U, failures = 0U;
void Check(bool ok, const char* name) {
    ++cases;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
void Put32(std::vector<std::uint8_t>& image, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0U; i < 4U; ++i) image[at + i] = static_cast<std::uint8_t>(value >> (i * 8U));
}
void Seal(std::vector<std::uint8_t>& image) {
    Put32(image, image.size() - 4U, firmware_upgrade::firmware_update::Crc32(image.data(), image.size() - 4U));
}
class UpdateSpy : public firmware_upgrade::f13_boot::UpdateClient {
public:
    unsigned calls = 0U;
    bool StageMetadata(const std::vector<std::uint8_t>&, const std::function<bool()>&, std::wstring*) override { ++calls; return false; }
    bool Start(const firmware_upgrade::f13_boot::ImageInfo&, firmware_upgrade::f13_boot::Ack*, std::wstring*) override { ++calls; return false; }
    bool SendBlock(std::uint32_t, const std::vector<std::uint8_t>&, firmware_upgrade::f13_boot::Ack*, std::wstring*) override { ++calls; return false; }
    bool End(std::uint32_t, std::uint32_t, firmware_upgrade::f13_boot::Ack*, std::wstring*) override { ++calls; return false; }
    bool Abort(firmware_upgrade::f13_boot::Ack*, std::wstring*) override { ++calls; return false; }
};
}
int wmain(int argc, wchar_t** argv) {
    using namespace firmware_upgrade;
    using namespace firmware_update;
    if (argc < 2) return 2;
    for (int arg = 1; arg < argc; ++arg) {
        std::ifstream source(std::filesystem::path(argv[arg]), std::ios::binary);
        std::vector<std::uint8_t> image(std::istreambuf_iterator<char>(source), {});
        const auto inspected = InspectImage(image);
        ImageInfo info;
        std::wstring error;
        Check(inspected.metadata_valid && ValidateImage(inspected.metadata.profile, image, &info, &error), "real current golden image accepted");
        if (!inspected.metadata_valid) { std::wcerr << error << L' ' << inspected.metadata_error << L'\n'; continue; }
        const bool rh850 = info.policy.irq_abi == 0x31563852U;
        const auto profile = info.profile;
        ImageInfo parsed;
        std::vector<std::size_t> offsets;
        if (rh850) {
            offsets.push_back(0U);
            for (std::size_t at = 0x200U; at < 0x400U; at += 16U) offsets.push_back(at);
            for (std::size_t at = 0x400U; at < 0xC00U; at += 4U) offsets.push_back(at);
        } else {
            for (std::size_t at = 0U; at < 256U; at += 4U) {
                if (at == 4U && info.policy.irq_abi != 0x31563246U) continue;
                offsets.push_back(at);
            }
        }
        for (auto at : offsets) {
            auto bad = image;
            if (rh850 && at >= 0x400U) Put32(bad, at, info.application_start);
            else bad[at] = 0U;
            Seal(bad);
            Check(!ValidateImage(profile, bad, &parsed, &error), "CRC-correct invalid entry/IRQ rejected in PC preflight");
            if (profile == Profile::CanUpgrade && at == 0U) {
                UpdateSpy device;
                f13_boot::UpdateReport report;
                Check(!f13_boot::RunUpdate(&device, bad, {}, &report, &error) &&
                    report.result == f13_boot::UpdateResult::InvalidImage && device.calls == 0U,
                    "production CAN update rejects invalid vectors before metadata staging, START or ABORT");
            }
            auto out_of_range = image;
            if (rh850 && at >= 0x400U) Put32(out_of_range, at, info.application_start + static_cast<std::uint32_t>(image.size()));
            else if (!rh850) {
                out_of_range[at + 1U] = out_of_range[at + 2U] = out_of_range[at + 3U] = 0xFFU;
            } else continue;
            Seal(out_of_range);
            Check(!ValidateImage(profile, out_of_range, &parsed, &error), "CRC-correct out-of-range ISR rejected");
        }
        auto padded = image;
        padded.resize(info.application_capacity, 0xFFU);
        Put32(padded, 0x118U, static_cast<std::uint32_t>(padded.size()));
        Seal(padded);
        Check(ValidateImage(profile, padded, &parsed, &error), "full-capacity padded timing BIN accepted");
        auto unknown = image;
        Put32(unknown, 0x168U, 0x41424939U);
        Seal(unknown);
        Check(ValidateImage(profile, unknown, &parsed, &error) == (profile == Profile::CanUpgrade),
            "unknown CAN ABI keeps live/device gates; serial remains strict");
    }
    ui::ReportStatus report;
    Check(report.Path().empty() && report.Summary().empty(), "initial report state has no fabricated evidence");
    report.Begin(1U);
    Check(report.pending && report.Path().empty(), "new report operation starts independently");
    report.Save(1U, L"one.json", L"one.html", L"");
    Check(report.Path() == L"one.html" && report.error.empty(), "current HTML belongs to successful pair");
    report.Begin(2U);
    report.Save(2U, L"", L"", L"disk denied");
    Check(report.Path() == L"one.html" && std::wstring(report.Button()) == L"Open Previous Report" &&
        report.Details().find(L"disk denied") != std::wstring::npos, "failure clearly labels previous operation and exact save error");
    report.Begin(3U);
    report.Save(3U, L"three.json", L"", L"HTML denied");
    Check(report.Path() == L"three.json" && std::wstring(report.Button()) == L"Open Report JSON", "JSON-only current report takes precedence over old HTML");
    report.Begin(4U);
    report.Save(4U, L"", L"", L"no report");
    Check(report.Path() == L"three.json" && std::wstring(report.Button()) == L"Open Previous Report", "previous JSON remains identifiable");
    report.Begin(5U);
    report.Save(5U, L"", L"five.html", L"JSON denied");
    Check(report.Path() == L"five.html" && report.Summary().find(L"JSON was not saved") != std::wstring::npos, "HTML-only current report remains usable");
    std::cout << "Follow-up core: " << cases << " checks, " << failures << " failures; no hardware\n";
    return failures ? 1 : 0;
}
