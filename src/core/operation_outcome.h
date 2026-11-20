#pragma once
#include <string>

namespace firmware_upgrade::operation {
enum class Outcome { Unspecified, Failed, Canceled, Succeeded };

// Classify once at the workflow boundary. A late cancel cannot hide an I/O
// failure or undo an operation that has already succeeded. Device side effects
// remain in the transport's independent program evidence.
inline Outcome Classify(bool success, bool cancel_requested, const std::wstring& error) {
    if (success) return Outcome::Succeeded;
    if (cancel_requested && (error.empty() || error.find(L"canceled") != std::wstring::npos ||
                             error.find(L"cancelled") != std::wstring::npos)) {
        return Outcome::Canceled;
    }
    return Outcome::Failed;
}

inline const wchar_t* Label(Outcome outcome) {
    switch (outcome) {
    case Outcome::Succeeded: return L"PASS";
    case Outcome::Canceled: return L"CANCELED";
    default: return L"FAIL";
    }
}
} // namespace firmware_upgrade::operation
