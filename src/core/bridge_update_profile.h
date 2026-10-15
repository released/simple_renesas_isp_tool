#pragma once
#include "host_link_client.h"
#include "firmware_update_profile.h"
#include "../../firmware/common/bridge_update_contract.h"
#include <functional>

namespace firmware_upgrade::bridge_update {
struct Image {
    bool available=false, valid=false, crc_matches=false;
    std::uint64_t size=0;
    std::uint32_t calculated_crc=0, stored_crc=0;
    bu_manifest_t metadata={};
    std::wstring error;
};
Image Inspect(const std::vector<std::uint8_t>& bytes);
bool ReadInfo(host_link::Client& link, bu_info_t* info, std::wstring* error);
bool ReadMetadata(host_link::Client& link, std::vector<std::uint8_t>* region, std::wstring* error);
bool EnterBoot(host_link::Client& link, std::uint16_t board, std::wstring* error);
bool Program(host_link::Client& link, const bu_info_t& device,
             const std::vector<std::uint8_t>& bytes,
             const std::function<bool()>& cancelled,
             const std::function<void(int)>& progress,
             const std::function<void()>& abort_transaction, std::wstring* error);
} // namespace firmware_upgrade::bridge_update
