#pragma once
#include <cstdint>

namespace firmware_upgrade::evidence {

constexpr std::uint32_t kMinimumRestartQuietMs=1000U;
constexpr std::uint32_t kMaximumRestartQuietMs=6000U;
constexpr std::uint32_t kRestartSizeMsPerKib=8U;
constexpr std::uint32_t kRestartMarginMs=500U;

// Conservative size estimate for the current device samples, not an MCU limit.
// Measured END time also protects slower devices; all scheduling stays bounded.
inline std::uint32_t DeviceRestartQuietMs(std::uint32_t image_bytes,
    std::uint64_t measured_crc_ms=0U, bool entering_boot=false) {
    if (image_bytes==0U || image_bytes==0xFFFFFFFFU) return kMaximumRestartQuietMs;
    const std::uint64_t kib=(static_cast<std::uint64_t>(image_bytes)+1023U)/1024U;
    const std::uint64_t size_ms=kib*kRestartSizeMsPerKib;
    const auto measured=measured_crc_ms>kMaximumRestartQuietMs ? kMaximumRestartQuietMs : measured_crc_ms;
    const auto one_pass=size_ms>measured ? size_ms : measured;
    // ENTER may validate once before CAN starts and again in the Boot session.
    const auto requested=one_pass*(entering_boot ? 2U : 1U)+kRestartMarginMs;
    return requested<kMinimumRestartQuietMs ? kMinimumRestartQuietMs :
        requested>kMaximumRestartQuietMs ? kMaximumRestartQuietMs :
        static_cast<std::uint32_t>(requested);
}

// PC scheduling only: no change to CAN retries, device timing or image gates.
class AppConfirmationTiming {
public:
    static constexpr std::uint32_t quiet_ms = kMinimumRestartQuietMs;
    static constexpr std::uint32_t budget_ms = 12000U;
    static constexpr std::uint32_t poll_ms = 100U;
    static constexpr std::uint32_t cancel_slice_ms = 20U;
    enum class Action { Wait, Probe, Expired, Canceled };

    explicit AppConfirmationTiming(std::uint32_t initial_quiet_ms=quiet_ms)
        : quiet_ms_(initial_quiet_ms<quiet_ms ? quiet_ms :
            initial_quiet_ms>kMaximumRestartQuietMs ? kMaximumRestartQuietMs : initial_quiet_ms),
          next_probe_ms_(quiet_ms_) {}
    std::uint32_t QuietMs() const { return quiet_ms_; }
    Action Next(std::uint64_t elapsed_ms, bool canceled) const {
        if (canceled) return Action::Canceled;
        if (elapsed_ms >= budget_ms) return Action::Expired;
        return elapsed_ms < next_probe_ms_ ? Action::Wait : Action::Probe;
    }
    void ProbeFinished(std::uint64_t elapsed_ms) {
        next_probe_ms_ = elapsed_ms + poll_ms;
    }
private:
    std::uint32_t quiet_ms_;
    std::uint64_t next_probe_ms_;
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
