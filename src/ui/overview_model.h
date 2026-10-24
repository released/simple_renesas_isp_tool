#pragma once
#include "../core/host_link_client.h"
#include "../core/f13_boot_profile.h"
#include "../core/f13_transfer_profile.h"
#include "../core/firmware_update_profile.h"
#include "../core/g16_i2c_reference.h"
#include "../core/bridge_update_profile.h"
#include "../core/isp_device_client.h"
#include <array>

namespace firmware_upgrade::overview {
enum class Signal { Unavailable, Verified, Reported, Offline, Invalid };
enum class TargetKind { None, Transfer, CanUpgrade, G16Reference };
struct Card {
    std::wstring heading, value, detail;
    Signal signal = Signal::Unavailable;
};
struct Input {
    bool host_current = false, target_current = false, i2c = false;
    bool misc = false, bridge_current = false;
    bool uart=false;
    isp::TargetInfo uart_target;
    bu_info_t bridge = {};
    bridge_update::Image bridge_image;
    host_link::HostInfo host;
    std::wstring port;
    TargetKind kind = TargetKind::None;
    f13_boot::TargetInfo target;
    f13_transfer::DeviceInfo transfer;
    g16_i2c::DeviceInfo g16;
    std::uint64_t bin_size = 0U;
    firmware_update::ImageInspection image;
    g16_i2c::ImageInspection i2c_image;
};
// Presentation only: these cards never authorize programming or infer a
// silicon part from an adapter name. Live INFO is not flash read-back/auth.
std::array<Card, 5> Build(const Input& input);
} // namespace firmware_upgrade::overview
