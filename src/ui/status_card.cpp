#include "status_card.h"

namespace firmware_upgrade::visual {
Palette Colors(Tone tone) {
    switch (tone) {
        case Tone::Ready: return {RGB(230,255,240), RGB(0,85,43), RGB(0,145,80)};
        case Tone::Pending: return {RGB(255,246,216), RGB(112,64,0), RGB(202,130,0)};
        case Tone::Error: return {RGB(255,234,237), RGB(163,14,39), RGB(213,30,63)};
        default: return {GetSysColor(COLOR_3DFACE),GetSysColor(COLOR_BTNTEXT),GetSysColor(COLOR_3DSHADOW)};
    }
}
} // namespace firmware_upgrade::visual
BEGIN_MESSAGE_MAP(CStatusCard, CStatic)
    ON_WM_PAINT()
END_MESSAGE_MAP()
void CStatusCard::SetTone(firmware_upgrade::visual::Tone tone) {
    if (tone_ == tone) return;
    tone_ = tone;
    if (GetSafeHwnd() != nullptr) Invalidate(FALSE);
}
void CStatusCard::SetEmphasized(bool emphasized) {
    if (emphasized_ == emphasized) return;
    emphasized_ = emphasized;
    if (GetSafeHwnd() != nullptr) Invalidate(FALSE);
}
void CStatusCard::OnPaint() {
    CPaintDC dc(this);
    CRect rectangle; GetClientRect(&rectangle);
    const auto palette = firmware_upgrade::visual::Colors(tone_);
    dc.FillSolidRect(rectangle, info_style_ && tone_!=firmware_upgrade::visual::Tone::Error ? GetSysColor(COLOR_WINDOW) : palette.background);
    const auto border=info_style_ ? GetSysColor(COLOR_3DSHADOW) : palette.border;
    dc.Draw3dRect(rectangle, border, border);
    dc.FillSolidRect(rectangle.left, rectangle.top, info_style_ ? 3 : 5, rectangle.Height(), palette.border);
    rectangle.DeflateRect(10, 3, 6, 3);
    CString text; GetWindowTextW(text);
    CFont* previous = GetFont() == nullptr ? nullptr : dc.SelectObject(GetFont());
    dc.SetBkMode(TRANSPARENT); dc.SetTextColor(info_style_ ? GetSysColor(COLOR_WINDOWTEXT) : palette.text);
    CFont emphasis_font;
    CFont* before_emphasis = nullptr;
    if (emphasized_ && heading_.empty()) {
        LOGFONT spec = {};
        if (GetFont() != nullptr && GetFont()->GetLogFont(&spec)) {
            spec.lfWeight = FW_BOLD;
            if (emphasis_font.CreateFontIndirect(&spec))
                before_emphasis = dc.SelectObject(&emphasis_font);
        }
        dc.SetTextColor(RGB(0,0,0));
    }
    if (heading_.empty()) {
        dc.DrawTextW(text, rectangle, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    } else {
        TEXTMETRIC metric = {}; dc.GetTextMetrics(&metric);
        const int line = metric.tmHeight + 1;
        CRect heading_rect = rectangle; heading_rect.bottom = heading_rect.top + line;
        dc.DrawTextW(heading_.c_str(), heading_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        LOGFONT spec = {};
        CFont headline;
        if (GetFont() != nullptr && GetFont()->GetLogFont(&spec)) {
            spec.lfWeight = FW_BOLD;
            headline.CreateFontIndirect(&spec);
        }
        CFont* normal = headline.GetSafeHandle() != nullptr ? dc.SelectObject(&headline) : nullptr;
        CRect value_rect = rectangle;
        value_rect.top += line; value_rect.bottom = value_rect.top + line;
        dc.DrawTextW(value_.c_str(), value_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (normal != nullptr) dc.SelectObject(normal);
        rectangle.top = value_rect.bottom;
        dc.DrawTextW(detail_.c_str(), rectangle, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (before_emphasis != nullptr) dc.SelectObject(before_emphasis);
    if (previous != nullptr) dc.SelectObject(previous);
}
void CStatusCard::SetContent(const std::wstring& heading, const std::wstring& value,
                             const std::wstring& detail) {
    if (heading_ == heading && value_ == value && detail_ == detail) return;
    heading_ = heading; value_ = value; detail_ = detail;
    if (GetSafeHwnd() != nullptr) {
        SetWindowTextW((heading + L"\r\n" + value + L"\r\n" + detail).c_str());
        Invalidate(FALSE);
    }
}
