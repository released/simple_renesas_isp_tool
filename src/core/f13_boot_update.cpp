#include "f13_boot_update.h"

#include <array>
#include <algorithm>
#include <chrono>

namespace firmware_upgrade::f13_boot {

namespace {

bool Fail(const wchar_t* text, std::wstring* error) {
    if (error != nullptr) {
        *error = text;
    }
    return false;
}

void AbortAfterStart(UpdateClient* client,
                     UpdateReport* report,
                     std::wstring* error, const UpdateCallbacks& callbacks) {
    Ack abort_ack;
    std::wstring abort_error;
    report->abort_attempted = true;
    report->abort_acknowledged = callbacks.abort ? callbacks.abort(&abort_ack,&abort_error) :
        client->Abort(&abort_ack, &abort_error);
    if (!report->abort_acknowledged && error != nullptr &&
        !abort_error.empty()) {
        if (!error->empty()) {
            *error += L" ";
        }
        *error += L"Cleanup ABORT failed: " + abort_error;
    }
}

bool IsCanceled(const UpdateCallbacks& callbacks) {
    return callbacks.is_canceled && callbacks.is_canceled();
}

class PhaseTimer {
public:
    explicit PhaseTimer(std::uint64_t& result) : result_(result) {}
    ~PhaseTimer() {
        result_ = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_).count());
    }
private:
    std::uint64_t& result_;
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
};

} // namespace

bool RunUpdate(UpdateClient* client,
               const std::vector<std::uint8_t>& image,
               const UpdateCallbacks& callbacks,
               UpdateReport* report,
               std::wstring* error) {
    if (client == nullptr || report == nullptr) {
        return Fail(L"F13 update runner input is null.", error);
    }

    *report = {};
    if (!ParseUpdateImage(image, &report->image, error)) {
        report->result = UpdateResult::InvalidImage;
        return false;
    }
    if (IsCanceled(callbacks)) {
        report->result = UpdateResult::Canceled;
        return Fail(L"F13 update canceled before START.", error);
    }

    Ack ack;
    if (callbacks.stage) callbacks.stage(UpdateStage::Metadata);
    {
    PhaseTimer timer(report->metadata_ms);
    if (!client->StageMetadata(image,callbacks.is_canceled,error)) {
        report->result=IsCanceled(callbacks)?UpdateResult::Canceled:UpdateResult::StartFailed;
        AbortAfterStart(client,report,error,callbacks);
        return false;
    }
    }
    if (IsCanceled(callbacks)) {
        report->result=UpdateResult::Canceled;
        AbortAfterStart(client,report,error,callbacks);
        return Fail(L"F13 update canceled after metadata, before START.",error);
    }
    if (callbacks.stage) callbacks.stage(UpdateStage::Start);
    report->image.force_old_version=callbacks.force_old_version;
    {
    PhaseTimer timer(report->start_ms);
    if (!client->Start(report->image, &ack, error)) {
        report->result = UpdateResult::StartFailed;
        /* START may have reached the target even when its ACK was lost. */
        AbortAfterStart(client, report, error, callbacks);
        return false;
    }
    report->start_acknowledged = true;
    }

    if (callbacks.stage) callbacks.stage(UpdateStage::Transfer);
    {
    PhaseTimer timer(report->transfer_ms);
    const std::uint32_t block_count =
        (report->image.image_size + kBlockBytes - 1U) / kBlockBytes;
    for (std::uint32_t block = 0U; block < block_count; ++block) {
        if (IsCanceled(callbacks)) {
            report->result = UpdateResult::Canceled;
            if (error != nullptr) {
                *error = L"F13 update canceled during DATA transfer.";
            }
            AbortAfterStart(client, report, error, callbacks);
            return false;
        }

        const std::size_t offset = static_cast<std::size_t>(block * kBlockBytes);
        const std::size_t end = (std::min)(image.size(), offset + kBlockBytes);
        const std::vector<std::uint8_t> data(image.begin() + offset, image.begin() + end);
        if (!client->SendBlock(block * kBlockBytes, data,
                              &ack, error)) {
            report->result = UpdateResult::DataFailed;
            AbortAfterStart(client, report, error, callbacks);
            return false;
        }
        report->blocks_acked = block + 1U;
        if (callbacks.progress &&
            (((block + 1U) % 16U) == 0U || block + 1U == block_count)) {
            callbacks.progress(static_cast<std::uint32_t>(end), report->image.image_size);
        }
    }
    }

    if (IsCanceled(callbacks)) {
        report->result = UpdateResult::Canceled;
        if (error != nullptr) {
            *error = L"F13 update canceled before END.";
        }
        AbortAfterStart(client, report, error, callbacks);
        return false;
    }
    if (callbacks.stage) callbacks.stage(UpdateStage::End);
    {
    PhaseTimer timer(report->end_ms);
    if (!client->End(report->image.calculated_crc32,
                     report->image.image_size, &ack, error)) {
        report->result = UpdateResult::EndFailed;
        AbortAfterStart(client, report, error, callbacks);
        return false;
    }
    }

    report->result = UpdateResult::Success;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace firmware_upgrade::f13_boot
