#pragma once
#include <cstdint>
#include <string>

namespace firmware_upgrade::ui {
// Storage status is independent of the device operation's success/failure.
struct ReportStatus {
    std::uint64_t operation = 0;
    bool pending = false;
    std::wstring json, html, error, previous_path;

    void Begin(std::uint64_t id) {
        if (!html.empty() || !json.empty()) {
            previous_path = !html.empty() ? html : json;
        }
        operation = id;
        pending = true;
        json.clear();
        html.clear();
        error.clear();
    }
    void Save(std::uint64_t id, const std::wstring& json_path,
              const std::wstring& html_path, const std::wstring& failure) {
        if (id != operation) {
            Begin(id);
        }
        pending = false;
        json = json_path;
        html = html_path;
        error = failure;
    }
    std::wstring Path() const {
        return !html.empty() ? html : !json.empty() ? json : previous_path;
    }
    const wchar_t* Button() const {
        return !html.empty() ? L"Open Last Report" : !json.empty() ? L"Open Report JSON" :
            !previous_path.empty() ? L"Open Previous Report" : L"Open Last Report";
    }
    std::wstring Summary() const {
        if (!operation) {
            return L"";
        }
        if (pending) {
            return L"Report: preparing for this operation.";
        }
        if (!html.empty() && !json.empty()) {
            return L"Report: JSON / HTML saved for this operation.";
        }
        return !json.empty() ? L"Report: JSON saved; HTML was not saved." :
            !html.empty() ? L"Report: HTML saved; JSON was not saved." :
            !previous_path.empty() ? L"Report: not saved; the button opens a previous operation." :
            L"Report: not saved for this operation.";
    }
    std::wstring Details() const {
        const auto summary = Summary();
        if (summary.empty()) {
            return summary;
        }
        return summary + L"\r\nOperation: " + std::to_wstring(operation) +
            (json.empty() ? L"" : L"\r\nJSON: " + json) +
            (html.empty() ? L"" : L"\r\nHTML: " + html) +
            (error.empty() ? L"" : L"\r\nSave error: " + error);
    }
};
}
