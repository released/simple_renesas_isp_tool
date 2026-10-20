#pragma once
#include <cstdint>

namespace firmware_upgrade::evidence {

// PC scheduling only: no change to CAN retries, device timing or image gates.
class AppConfirmationTiming {
public:
    static constexpr std::uint32_t quiet_ms = 1000U;
    static constexpr std::uint32_t budget_ms = 12000U;
    static constexpr std::uint32_t poll_ms = 100U;
    static constexpr std::uint32_t cancel_slice_ms = 20U;
    enum class Action { Wait, Probe, Expired, Canceled };

    Action Next(std::uint64_t elapsed_ms, bool canceled) const {
        if (canceled) return Action::Canceled;
        if (elapsed_ms >= budget_ms) return Action::Expired;
        return elapsed_ms < next_probe_ms_ ? Action::Wait : Action::Probe;
    }
    void ProbeFinished(std::uint64_t elapsed_ms) {
        next_probe_ms_ = elapsed_ms + poll_ms;
    }
private:
    std::uint64_t next_probe_ms_ = quiet_ms;
};

// The worker supplies a monotonic clock and cancel-aware, bounded sleep slices.
// Tests use this same wait loop without a COM port or wall-clock delay.
template<class Elapsed, class Canceled, class SleepSlice>
AppConfirmationTiming::Action AwaitAppProbe(const AppConfirmationTiming& timing,
    Elapsed elapsed, Canceled canceled, SleepSlice sleep_slice) {
    for (;;) {
        const auto action = timing.Next(elapsed(), canceled());
        if (action != AppConfirmationTiming::Action::Wait) return action;
        sleep_slice(AppConfirmationTiming::cancel_slice_ms);
    }
}
} // namespace firmware_upgrade::evidence
