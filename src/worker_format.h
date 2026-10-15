#pragma once
#include "worker_session.h"
#include "core/firmware_update_profile.h"
namespace firmware_upgrade::ui::worker_detail {
std::wstring Hex16(std::uint16_t value);
std::wstring Hex32(std::uint32_t value);
std::wstring NarrowAsciiToWide(const std::string& value);
std::wstring HexBytes(const std::vector<std::uint8_t>& bytes);
std::wstring CanChannelSourcesText(std::uint16_t sources);
std::wstring CanGlobalSourcesText(std::uint16_t sources);
std::wstring VersionText(std::uint32_t version);
bool ImageMatchesTarget(const firmware_update::ImageInfo& image,
                        const f13_boot::TargetInfo& target, std::wstring* error);
std::uint16_t ReadLe16(const std::uint8_t* data);
std::uint32_t ReadLe32(const std::uint8_t* data);
}
