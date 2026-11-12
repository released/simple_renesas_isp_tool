#pragma once

#include <string>

namespace firmware_upgrade::feedback {

// These are verified operation outcomes, not transfer-percentage states.
enum class Result {
    None,
    DeviceAppConfirmed,
    UartAppConfirmed,
    UartAppConfirmedRxWarning,
    DeviceAppConfirmedBusWarning,
    DeviceAppConfirmedCanActive,
    HostAppWritten,
    HostAppRunning,
};

struct View {
    std::wstring title;
    std::wstring status;
    std::wstring next;
    std::wstring progress;
    std::wstring compact;
    bool warning = false;
};

inline bool IsDevice(Result result) {
    return result == Result::UartAppConfirmed || result == Result::UartAppConfirmedRxWarning || result == Result::DeviceAppConfirmed ||
           result == Result::DeviceAppConfirmedBusWarning ||
           result == Result::DeviceAppConfirmedCanActive;
}

inline Result WithHostCounterWarning(Result result, bool increased) {
    return increased && result == Result::DeviceAppConfirmed ?
        Result::DeviceAppConfirmedBusWarning : result;
}

inline View Describe(Result result) {
    switch (result) {
    case Result::UartAppConfirmedRxWarning:
        return {L"UART device App updated / RX counter warning",
            L"[COMPLETE / WARNING] App CRC and startup confirmed; UART1 RX error/drop counters increased.",
            L"Review UART1 counters and Open Last Report. Do not repeat the update to clear counters.",
            L"UART App confirmed", L"Check UART0 Log / Open Last Report. COM open.", true};
    case Result::UartAppConfirmed:
        return {L"Device App update complete",
            L"[COMPLETE] App CRC and startup confirmed; device bus released; COM open.",
            L"Check App operation and UART0 Log. Next update: Discover Target.",
            L"ISP device update complete", L"Check App operation / UART0 Log. Next update: Discover Target."};
    case Result::DeviceAppConfirmed:
        return {L"Device App update complete",
            L"[COMPLETE] App CRC and startup confirmed; CAN stopped; COM open.",
            L"Check App operation. Next update: Discover Target.", L"Device update complete",
            L"Check App operation. Next update: Discover Target."};
    case Result::DeviceAppConfirmedBusWarning:
        return {L"Device App updated / Host counter warning",
            L"[COMPLETE / WARNING] App CRC and startup confirmed; CAN stopped; Host error counters increased.",
            L"Review Host counters in Open Last Report. COM open. Do not repeat the update to clear the warning.",
            L"Device App confirmed", L"Review Host counters / Open Last Report. Do not repeat the update to clear the warning.", true};
    case Result::DeviceAppConfirmedCanActive:
        return {L"Device App updated / CAN stop not confirmed",
            L"[COMPLETE / WARNING] App CRC and startup confirmed; CAN stop unconfirmed.",
            L"CAN may still be active. Close COM before changing CAN settings. Do not repeat the update to stop CAN.",
            L"Device App confirmed",
            L"CAN may still be active. Close COM before changing CAN settings. Do not repeat the update to stop CAN.", true};
    case Result::HostAppWritten:
        return {L"Host App update complete - RUN required",
            L"[COMPLETE] Host App written; CRC and metadata verified; still in Boot.",
            L"Next: Run Validated Bridge App. If unavailable, Read Bridge Update Info first.",
            L"Host update complete", L"Next: Run Validated Bridge App. App has not started."};
    case Result::HostAppRunning:
        return {L"Host App startup confirmed",
            L"[COMPLETE] Host App running; same-COM reconnect and image identity confirmed.",
            L"Check Host App operation or select a device interface.", L"Host App confirmed",
            L"Check Host App operation or select a device interface."};
    default:
        return {};
    }
}

inline std::wstring ReportResult(bool success, bool canceled, Result result) {
    if (!success) return canceled ? L"CANCELED" : L"FAIL";
    return Describe(result).warning ? L"PASS_WITH_WARNINGS" : L"PASS";
}

inline std::wstring CanNextStep(bool host, bool discovered, bool boot) {
    if (!host) return L"Next: Open + Probe Host.";
    if (!discovered) return L"Next: Discover Target.";
    if (!boot) return L"Next: Enter Boot. Start Device Update before the Boot inactivity window expires.";
    return L"Boot ready. Load a matching BIN to update.";
}

// Image-setting blockers precede discovery/ENTER hints; they do not change gates.
inline std::wstring CanImageBlocker(bool mode_supported, bool bitrate_supported) {
    if (!mode_supported) {
        return L"[BLOCKED] BIN does not support the selected CAN mode.\r\n"
               L"Match Interface / Rate to device Boot and CAN Mode.";
    }
    if (!bitrate_supported) {
        return L"[BLOCKED] Selected CAN bitrate is not supported by the image.\r\n"
               L"Match Interface / Rate to device Boot and CAN settings.";
    }
    return {};
}

} // namespace firmware_upgrade::feedback
