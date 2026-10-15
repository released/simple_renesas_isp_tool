#include "f13_boot_update.h"

#include <array>
#include <algorithm>

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
                     std::wstring* error) {
    Ack abort_ack;
    std::wstring abort_error;
    report->abort_attempted = true;
    report->abort_acknowledged = client->Abort(&abort_ack, &abort_error);
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
    if (!client->StageMetadata(image,callbacks.is_canceled,error)) {
        report->result=IsCanceled(callbacks)?UpdateResult::Canceled:UpdateResult::StartFailed;
        AbortAfterStart(client,report,error);
        return false;
    }
    if (IsCanceled(callbacks)) {
        report->result=UpdateResult::Canceled;
        AbortAfterStart(client,report,error);
        return Fail(L"F13 update canceled after metadata, before START.",error);
    }
    if (!client->Start(report->image, &ack, error)) {
        report->result = UpdateResult::StartFailed;
        /* START may have reached the target even when its ACK was lost. */
        AbortAfterStart(client, report, error);
        return false;
    }
    report->start_acknowledged = true;

    const std::uint32_t block_count =
        (report->image.image_size + kBlockBytes - 1U) / kBlockBytes;
    for (std::uint32_t block = 0U; block < block_count; ++block) {
        if (IsCanceled(callbacks)) {
            report->result = UpdateResult::Canceled;
            if (error != nullptr) {
                *error = L"F13 update canceled during DATA transfer.";
            }
            AbortAfterStart(client, report, error);
            return false;
        }

        const std::size_t offset = static_cast<std::size_t>(block * kBlockBytes);
        const std::size_t end = (std::min)(image.size(), offset + kBlockBytes);
        const std::vector<std::uint8_t> data(image.begin() + offset, image.begin() + end);
        if (!client->SendBlock(block * kBlockBytes, data,
                              &ack, error)) {
            report->result = UpdateResult::DataFailed;
            AbortAfterStart(client, report, error);
            return false;
        }
        report->blocks_acked = block + 1U;
        if (callbacks.progress &&
            (((block + 1U) % 16U) == 0U || block + 1U == block_count)) {
            callbacks.progress(static_cast<std::uint32_t>(end), report->image.image_size);
        }
    }

    if (IsCanceled(callbacks)) {
        report->result = UpdateResult::Canceled;
        if (error != nullptr) {
            *error = L"F13 update canceled before END.";
        }
        AbortAfterStart(client, report, error);
        return false;
    }
    if (!client->End(report->image.calculated_crc32,
                     report->image.image_size, &ack, error)) {
        report->result = UpdateResult::EndFailed;
        AbortAfterStart(client, report, error);
        return false;
    }

    report->result = UpdateResult::Success;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace firmware_upgrade::f13_boot
