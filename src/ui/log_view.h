#pragma once
#include <windows.h>
#include <string>

namespace firmware_upgrade::log_view {
void ScrollToEnd(HWND window);
// Appending must not change the reader's selection or viewport when disabled.
void Append(HWND window, const std::wstring& text, COLORREF color, bool follow);
} // namespace firmware_upgrade::log_view
