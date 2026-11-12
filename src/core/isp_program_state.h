#pragma once
#include <cstdint>

namespace firmware_upgrade::isp {
// Local evidence only. A missing reply never grants permission to replay a write.
enum class RequestOutcome { NotAttempted, Unconfirmed, Rejected, Acknowledged };
enum class ProgramStage { None, Discovery, Metadata, Start, Write, End, App };

struct ProgramState {
    ProgramStage last_confirmed = ProgramStage::None;
    RequestOutcome start = RequestOutcome::NotAttempted;
    RequestOutcome end = RequestOutcome::NotAttempted;
    std::uint64_t accepted_bytes = 0U;
    std::uint64_t end_elapsed_ms = 0U;

    bool MayAbortDevice() const { return end == RequestOutcome::NotAttempted; }
};

inline const wchar_t* OutcomeName(RequestOutcome outcome) {
    switch (outcome) {
    case RequestOutcome::Unconfirmed: return L"unconfirmed";
    case RequestOutcome::Rejected: return L"rejected";
    case RequestOutcome::Acknowledged: return L"acknowledged";
    default: return L"not_attempted";
    }
}
inline const wchar_t* StageName(ProgramStage stage) {
    switch (stage) {
    case ProgramStage::Discovery: return L"discovery";
    case ProgramStage::Metadata: return L"metadata";
    case ProgramStage::Start: return L"start";
    case ProgramStage::Write: return L"write";
    case ProgramStage::End: return L"end";
    case ProgramStage::App: return L"app_confirmed";
    default: return L"none";
    }
}
} // namespace firmware_upgrade::isp
