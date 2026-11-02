#pragma once
#include "firmware_update_profile.h"
#include "f13_boot_profile.h"

namespace firmware_upgrade::can_contract {
// One field policy serves UI snapshots, write-time live checks and App confirmation.
// Informational release differences do not block an ordinary update.
template<class Visitor>
void Visit(const firmware_update::ImageInfo& i, const f13_boot::TargetInfo& t, Visitor visit) {
    const auto equal = [&](const wchar_t* name, std::uint32_t a, std::uint32_t b, bool info = false) {
        visit(name, a, b, a == b, info);
    };
    equal(L"Target", i.target_id, t.target_id);
    visit(L"MCU", i.mcu_family + L" / " + i.mcu_part, t.mcu_family + L" / " + t.mcu_part,
        i.mcu_family == t.mcu_family && i.mcu_part == t.mcu_part, false);
    equal(L"App version", i.version, t.app_version, true);
    equal(L"Build", i.build, t.app_build, true);
    equal(L"Version control", i.policy.version_control, t.policy.version_control, true);
    equal(L"Image bytes", i.image_size, t.installed_app_size, true);
    equal(L"App start", i.application_start, t.application_start);
    equal(L"Capacity", i.application_capacity, t.application_capacity);
    equal(L"Flash bytes", i.code_flash_size, t.code_flash_size);
    equal(L"Boot reserve", i.boot_reserved_size, t.boot_reserved_size);
    equal(L"Erase bytes", i.erase_block_size, t.erase_block_size);
    equal(L"Minimum image", i.minimum_image_size, t.minimum_image_size);
    equal(L"Protocol / format", (i.protocol_id << 16U) | (i.protocol_version << 8U) | i.format,
        (t.protocol_id << 16U) | (t.protocol_version << 8U) | t.image_format);
    equal(L"Checksum", i.checksum_type, t.checksum_type);
    equal(L"CAN mode / ID type", (i.can_mode_mask << 8U) | i.can_id_format, (t.can_mode_mask << 8U) | t.can_id_format);
    equal(L"Nominal mask", i.nominal_bitrate_mask, t.nominal_bitrate_mask);
    equal(L"Data mask", i.data_bitrate_mask, t.data_bitrate_mask);
    equal(L"Request ID", i.request_id, t.request_id);
    equal(L"Response ID", i.response_id, t.response_id);
    visit(L"Image role", i.policy.role, t.policy.role,
        i.policy.role == IP_ROLE_DEVICE_APP && t.policy.role == IP_ROLE_DEVICE_APP, false);
    visit(L"Allowed CAN", i.policy.transports, t.policy.transports,
        (i.policy.transports & IP_CAN) != 0U && (t.policy.transports & IP_CAN) != 0U, false);
    equal(L"IRQ ABI", i.policy.irq_abi, t.policy.irq_abi);
    visit(L"Minimum Boot", i.policy.minimum_boot_version, t.boot_version,
        i.policy.minimum_boot_version <= t.boot_version, false);
}
inline bool Matches(const firmware_update::ImageInfo& image, const f13_boot::TargetInfo& target,
                    std::wstring* error = nullptr) {
    bool match = true;
    std::wstring first;
    Visit(image, target, [&](const wchar_t* field, const auto&, const auto&, bool passed, bool informational) {
        if (!passed && !informational) {
            match = false;
            if (first.empty()) first = field;
        }
    });
    if (error) *error = match ? L"" : L"Loaded image metadata does not match the discovered target: " + first + L".";
    return match;
}
} // namespace firmware_upgrade::can_contract
