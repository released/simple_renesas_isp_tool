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
Palette SourceColors(Provenance source) {
    switch(source) {
    case Provenance::HostLive: return {RGB(230,237,247),RGB(30,51,77),RGB(66,91,126)};
    case Provenance::DeviceLive: return {RGB(226,240,237),RGB(22,65,59),RGB(48,109,98)};
    case Provenance::ImageFile: return {RGB(247,238,219),RGB(85,60,22),RGB(145,107,47)};
    default: return {GetSysColor(COLOR_3DFACE),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_3DSHADOW)};
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
void CStatusCard::SetProvenance(firmware_upgrade::visual::Provenance source) {
    if(provenance_==source)return;
    provenance_=source;
    if(GetSafeHwnd()!=nullptr)Invalidate(FALSE);
}
std::vector<std::wstring> CStatusCard::CompactDetailLines() const {
    std::vector<std::wstring> lines;
    std::size_t start=0;
    // Keep the original detail for accessibility/tooltips; only paint two
    // single-line paragraphs below the heading/value in the compact BIN card.
    while(start<detail_.size() && lines.size()<2U) {
        const auto end=detail_.find(L'\n',start);
        auto text=detail_.substr(start,end==std::wstring::npos?end:end-start);
        if(!text.empty() && text.back()==L'\r')text.pop_back();
        if(lines.size()==1U && end!=std::wstring::npos && end+1U<detail_.size())text+=L" ...";
        lines.push_back(text);
        if(end==std::wstring::npos)break;
        start=end+1U;
    }
    return lines;
}
void CStatusCard::OnPaint() {
    CPaintDC dc(this);
    CRect rectangle; GetClientRect(&rectangle);
    const auto palette = firmware_upgrade::visual::Colors(tone_);
    dc.FillSolidRect(rectangle, info_style_ && tone_!=firmware_upgrade::visual::Tone::Error ? GetSysColor(COLOR_WINDOW) : palette.background);
    const auto source=firmware_upgrade::visual::SourceColors(provenance_);
    const auto border=info_style_ ? source.border : palette.border;
    dc.Draw3dRect(rectangle, border, border);
    if(info_style_) {
        CRect inner=rectangle;inner.DeflateRect(1,1);dc.Draw3dRect(inner,border,border);
        TEXTMETRIC metric={};
        auto* old_font=GetFont()?dc.SelectObject(GetFont()):nullptr;
        dc.GetTextMetrics(&metric);if(old_font)dc.SelectObject(old_font);
        dc.FillSolidRect(rectangle.left+2,rectangle.top+2,rectangle.Width()-4,metric.tmHeight+3,source.background);
    }
    dc.FillSolidRect(rectangle.left, rectangle.top, 5, rectangle.Height(), palette.border);
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
        dc.SetTextColor(info_style_?source.text:palette.text);
        dc.DrawTextW(heading_.c_str(), heading_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        dc.SetTextColor(info_style_?GetSysColor(COLOR_WINDOWTEXT):palette.text);
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
        if(compact_detail_) {
            for(const auto& detail_line:CompactDetailLines()) {
                CRect detail_rect=rectangle;
                detail_rect.bottom=detail_rect.top+line;
                if(detail_rect.bottom>rectangle.bottom)break;
                dc.DrawTextW(detail_line.c_str(),detail_rect,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                rectangle.top=detail_rect.bottom;
            }
        } else {
            dc.DrawTextW(detail_.c_str(), rectangle, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
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
