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
            L"[COMPLETE / WARNING] UART App written, CRC verified and startup confirmed; bridge UART1 RX error/drop counters increased.",
            L"The update succeeded. Review UART1 counter deltas, device UART0 Log and Open Last Report; do not repeat the update to clear counters.",
            L"UART App confirmed",L"App confirmed; UART1 RX counter warning. Check Log / Open Last Report; COM open.",true};
    case Result::UartAppConfirmed:
        return {L"Device App update complete",
            L"[COMPLETE] ISP device App written, CRC verified and startup confirmed. Host COM remains open.",
            L"Check App code and the device UART0 Log.\r\nNext update: Discover Target > Enter Boot (App mode) > Start Device Update.\r\nOpen Last Report shows the saved evidence.",
            L"ISP device update complete",L"App confirmed; device bus transaction released; COM open. Check App / UART0 Log."};
    case Result::DeviceAppConfirmed:
        return {L"Device App update complete",
            L"[COMPLETE] Device App written, CRC verified and App startup confirmed. CAN stopped; Host COM remains open.",
            L"You can now check your App code and board behavior.\r\n"
            L"To update again: Discover Target > Enter Boot (if App mode) > Start Device Update.\r\n"
            L"Open Last Report shows the saved operation evidence.", L"Device update complete",
            L"App confirmed; CAN stopped; COM open. Check App.\r\nNext update: Discover Target > Enter Boot (App mode)."};
    case Result::DeviceAppConfirmedBusWarning:
        return {L"Device App updated / Host counter warning",
            L"[COMPLETE / WARNING] Device App written, CRC verified and startup confirmed. CAN stopped; Host error counters increased.",
            L"The update succeeded. Review Host counter deltas and detailed trace in the Log / Open Last Report.\r\n"
            L"Host COM remains open. Do not repeat the update to clear the warning.",
            L"Device App confirmed",
            L"App confirmed; CAN stopped; Host counter warning.\r\nCheck Log / Open Last Report; COM open.", true};
    case Result::DeviceAppConfirmedCanActive:
        return {L"Device App updated / CAN stop not confirmed",
            L"[COMPLETE / WARNING] Device App verified and startup confirmed; Host CAN stop was not confirmed.",
            L"The update succeeded, but CAN may still be active. Check the stop warning in Shared Log.\r\n"
            L"Use Disconnect / Close COM before changing the CAN setup. Do not repeat the update to stop CAN.",
            L"Device App confirmed",
            L"App confirmed; CAN stop not confirmed.\r\nCheck Log; Disconnect / Close COM before setup changes.", true};
    case Result::HostAppWritten:
        return {L"Host App update complete - RUN required",
            L"[COMPLETE] Host App written; CRC and metadata readback verified. Host remains in Boot.",
            L"Next: Run Validated Bridge App to start the installed App.\r\n"
            L"The App has not been started yet. If RUN is unavailable, Read Bridge Update Info / Probe Host first.\r\n"
            L"Open Last Report shows the saved operation evidence.", L"Host update complete",
            L"Host App written and verified; still in Boot.\r\nNext: Run Validated Bridge App."};
    case Result::HostAppRunning:
        return {L"Host App startup confirmed",
            L"[COMPLETE] Host App is running. Same-COM reconnect and installed image identity verified.",
            L"You can now check the Host App or select a device interface.\r\n"
            L"Open Last Report shows the saved RUN confirmation.", L"Host App confirmed",
            L"Host App started; identity confirmed.\r\nCheck App or choose a device interface."};
    default:
        return {};
    }
}

inline std::wstring ReportResult(bool success, bool canceled, Result result) {
    if (!success) return canceled ? L"CANCELED" : L"FAIL";
    return Describe(result).warning ? L"PASS_WITH_WARNINGS" : L"PASS";
}

inline std::wstring CanNextStep(bool host, bool discovered, bool boot) {
    if (!host) return L"Next: Open + Probe Host, then Discover Target.";
    if (!discovered) return L"Next: Discover Target (available in Boot or App mode).";
    if (!boot) return L"Next: Enter Boot. App mode detected; then Start Device Update when the image is ready.";
    return L"Boot mode detected. Enter Boot is not needed; Start Device Update when the image is ready.";
}

// Image-setting blockers precede discovery/ENTER hints; they do not change gates.
inline std::wstring CanImageBlocker(bool mode_supported, bool bitrate_supported) {
    if (!mode_supported) {
        return L"[BLOCKED] Selected CAN mode is not supported by the image.\r\n"
               L"Check BIN supported CAN mode (Interface / Rate); load a BIN matching the device Boot and CAN Mode.";
    }
    if (!bitrate_supported) {
        return L"[BLOCKED] Selected CAN bitrate is not supported by the image.\r\n"
               L"Check BIN supported nominal/data bitrate (Interface / Rate) against the device Boot and CAN settings.";
    }
    return {};
}

} // namespace firmware_upgrade::feedback
