#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "f13_boot_profile.h"

namespace firmware_upgrade::f13_boot {

enum class UpdateResult {
    Success,
    InvalidImage,
    StartFailed,
    DataFailed,
    EndFailed,
    Canceled,
};

enum class UpdateStage { Metadata, Start, Transfer, End };

struct UpdateCallbacks {
    bool force_old_version = false;
    std::function<bool()> is_canceled;
    std::function<void(std::uint32_t completed,
                       std::uint32_t total)> progress;
    std::function<void(UpdateStage)> stage;
    std::function<bool(Ack*, std::wstring*)> abort;
};

struct UpdateReport {
    UpdateResult result = UpdateResult::InvalidImage;
    ImageInfo image;
    std::uint32_t blocks_acked = 0U;
    bool start_acknowledged = false;
    bool abort_attempted = false;
    bool abort_acknowledged = false;
    std::uint64_t metadata_ms = 0U;
    std::uint64_t start_ms = 0U;
    std::uint64_t transfer_ms = 0U;
    std::uint64_t end_ms = 0U;
};

bool RunUpdate(UpdateClient* client,
               const std::vector<std::uint8_t>& image,
               const UpdateCallbacks& callbacks,
               UpdateReport* report,
               std::wstring* error);

} // namespace firmware_upgrade::f13_boot
