#pragma once
#include "host_link_client.h"
#include "update_evidence.h"

namespace firmware_upgrade::evidence {
struct HostCounterEvidence {
    std::wstring deltas;
    bool increased = false;
    bool complete = true;
    std::wstring Status() const {
        return increased ? L"WARNING" : complete ? L"NO_RECORDED_ERRORS" : L"UNAVAILABLE";
    }
    std::wstring Note() const {
        if (increased) return L"Host error counters increased during this operation. "
            L"Review deltas and detailed trace; update/App confirmation is separate. "
            L"The counters do not identify the physical error cause.";
        if (!complete) return L"Exact Host error-counter comparison is unavailable "
            L"(missing snapshot, reset or saturation); this is not a clean-bus result.";
        return L"No increase in the recorded Host error counters. "
            L"This does not establish physical-layer or device-queue acceptance.";
    }
};

inline HostCounterEvidence ObserveHostCounters(const host_link::HostStats& before,
    const host_link::HostStats& after, bool comparable) {
    HostCounterEvidence result;
    const auto add = [&](const wchar_t* name, std::uint32_t a, std::uint32_t b,
                         std::uint32_t limit = 0xFFFFFFFFU) {
        std::wstring delta;
        if (!comparable) delta = L"unavailable (snapshot missing or host reset)";
        else if (a >= limit || b >= limit) delta = L"unavailable (saturated)";
        else if (b < a) delta = L"unavailable (reset)";
        else {
            delta = Delta(a, b);
            result.increased = result.increased || b > a;
        }
        if (delta.find(L"unavailable") != std::wstring::npos) result.complete = false;
        if (!result.deltas.empty()) result.deltas += L"; ";
        result.deltas += std::wstring(name) + L"=" + delta;
    };
    add(L"UART overflow", before.uart_rx_overflows, after.uart_rx_overflows, 65535U);
    add(L"CAN lost", before.can_rx_lost_frames, after.can_rx_lost_frames);
    add(L"CAN warning", before.can_warning_entries, after.can_warning_entries);
    add(L"CAN error-passive", before.can_error_passive_entries, after.can_error_passive_entries);
    add(L"CAN bus-off", before.can_bus_off_entries, after.can_bus_off_entries);
    add(L"CAN recovery", before.can_recovery_count, after.can_recovery_count);
    add(L"CAN TX failure", before.can_tx_failures, after.can_tx_failures);
    return result;
}
} // namespace firmware_upgrade::evidence
