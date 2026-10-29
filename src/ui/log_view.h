#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace firmware_upgrade::log_view {
void ScrollToEnd(HWND window);
// Appending must not change the reader's selection or viewport when disabled.
void Append(HWND window, const std::wstring& text, COLORREF color, bool follow);
struct Line {
    std::wstring text;
    COLORREF color;
};
void AppendBatch(HWND window, const std::vector<Line>& lines, bool follow);
} // namespace firmware_upgrade::log_view
