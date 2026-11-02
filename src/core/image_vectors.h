#pragma once
#include "firmware_update_profile.h"

namespace firmware_upgrade::firmware_update {
// IRQ architecture is independent of the update transport. R8V1 owns entry /
// EBASE branches and 512 INTBP pointers; RL78 owns 64 four-byte BR slots.
inline bool ValidateImageVectors(const std::vector<std::uint8_t>& image,
    const ImageInfo& info, std::wstring* error, bool require_known = false) {
    const auto fail = [&](const wchar_t* text) {
        if (error) {
            *error = text;
        }
        return false;
    };
    const auto abi = info.policy.irq_abi;
    const bool rh850 = abi == 0x31563852U;
    const bool known = rh850 || abi == 0x31563146U || abi == 0x31563246U ||
        abi == 0x31563647U || abi == 0x31563247U;
    // Unknown CAN ABIs retain live/device agreement checks. Serial stays strict.
    if (!known) {
        return !require_known || fail(L"Image IRQ ABI is not supported by this vector validator.");
    }
    if (image.size() < 516U) {
        return fail(L"Image is too short for metadata and vectors.");
    }
    if (info.minimum_image_size != (rh850 ? 0x1004U : 516U)) {
        return fail(L"Image minimum size differs from its IRQ architecture contract.");
    }
    const auto end = static_cast<std::uint64_t>(info.application_start) + image.size() - 4U;
    if (rh850) {
        const auto le32 = [&](std::size_t at) {
            return static_cast<std::uint32_t>(image[at]) |
                (static_cast<std::uint32_t>(image[at + 1U]) << 8U) |
                (static_cast<std::uint32_t>(image[at + 2U]) << 16U) |
                (static_cast<std::uint32_t>(image[at + 3U]) << 24U);
        };
        const auto branch = [&](std::size_t offset) {
            if (image[offset] == 0x1FU && !image[offset + 1U]) {
                offset += 2U;
            }
            if (image[offset] != 0xE0U || image[offset + 1U] != 2U) {
                return false;
            }
            const auto destination = info.application_start +
                static_cast<std::uint32_t>(offset) + le32(offset + 2U);
            return !(destination & 1U) && destination >=
                static_cast<std::uint64_t>(info.application_start) + 0x1000U && destination < end;
        };
        if (image.size() < 0x1004U || !branch(0U)) {
            return fail(L"RH850 App entry branch is invalid.");
        }
        for (std::size_t at = 0x200U; at < 0x400U; at += 16U) {
            if (!branch(at)) {
                return fail(L"RH850 EBASE exception stub is invalid.");
            }
        }
        for (std::size_t at = 0x400U; at < 0xC00U; at += 4U) {
            const auto destination = le32(at);
            if ((destination & 1U) || destination <
                static_cast<std::uint64_t>(info.application_start) + 0x1000U || destination >= end) {
                return fail(L"RH850 INTBP contains an invalid 512-entry ISR destination.");
            }
        }
        return true;
    }
    for (std::size_t at = 0U; at < 256U; at += 4U) {
        if (abi != 0x31563246U && at == 4U) {
            continue;
        }
        const auto* slot = image.data() + at;
        const std::uint32_t destination = slot[1] |
            (static_cast<std::uint32_t>(slot[2]) << 8U) |
            (static_cast<std::uint32_t>(slot[3]) << 16U);
        if (slot[0] != 0xECU || destination <
            static_cast<std::uint64_t>(info.application_start) + 512U || destination >= end) {
            return fail(L"App software vector table contains an invalid BR slot or ISR destination.");
        }
    }
    return true;
}
}
