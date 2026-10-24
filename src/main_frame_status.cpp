#include "main_frame.h"

namespace {
void Group(CDC& dc,const firmware_upgrade::console::Rect& box,const wchar_t* title,
           COLORREF border=GetSysColor(COLOR_3DSHADOW)) {
    if (box.width<=0 || box.height<=0) return;
    TEXTMETRIC metric={}; dc.GetTextMetrics(&metric);
    CPen pen(PS_SOLID,2,border);
    auto* old_pen=dc.SelectObject(&pen); auto* old_brush=dc.SelectStockObject(NULL_BRUSH);
    dc.Rectangle(box.x+1,box.y+metric.tmHeight/2,box.x+box.width-1,box.y+box.height-1);
    dc.SelectObject(old_pen); dc.SelectObject(old_brush);
    const auto size=dc.GetTextExtent(title);
    CRect caption(box.x+10,box.y,box.x+14+size.cx,box.y+metric.tmHeight+2);
    dc.FillSolidRect(caption,GetSysColor(COLOR_3DFACE));
    dc.DrawTextW(title,caption,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
}
}
void CMainFrame::InitializeThemeBrushes() {
    using firmware_upgrade::visual::Tone;
    CBrush* brushes[]={&page_background_brush_,&ready_background_brush_,&error_background_brush_,
        &pending_background_brush_,&neutral_background_brush_,&field_background_brush_};
    const COLORREF colors[]={GetSysColor(COLOR_3DFACE),firmware_upgrade::visual::Colors(Tone::Ready).background,
        firmware_upgrade::visual::Colors(Tone::Error).background,firmware_upgrade::visual::Colors(Tone::Pending).background,
        GetSysColor(COLOR_3DFACE),GetSysColor(COLOR_WINDOW)};
    for (int i=0;i<6;++i) {brushes[i]->DeleteObject(); brushes[i]->CreateSolidBrush(colors[i]);}
}
void CMainFrame::OnSysColorChange() {
    CFrameWnd::OnSysColorChange(); InitializeThemeBrushes();
    if (log_edit_.GetSafeHwnd()) log_edit_.SetBackgroundColor(FALSE,GetSysColor(COLOR_WINDOW));
    RedrawWindow(nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
void CMainFrame::OnPaint() {
    CPaintDC dc(this); dc.SetBkMode(TRANSPARENT); dc.SetTextColor(GetSysColor(COLOR_BTNTEXT));
    auto* previous=section_font_.GetSafeHandle() ? dc.SelectObject(&section_font_) : nullptr;
    const auto& layout=console_layout_;
    Group(dc,layout.connection_group,L"1. HOST CONNECTION");
    Group(dc,layout.analysis_group,L"2. INFORMATION - HOST LIVE / DEVICE LIVE / APP BIN OFFLINE");
    Group(dc,layout.interface_group,L"3. PERIPHERAL / TARGET");
    Group(dc,layout.image_group,L"4. LOAD / UPDATE IMAGE");
    Group(dc,layout.log_group,L"5. MESSAGE LOG");
    const wchar_t* titles[]={L"BIN check / layout",L"BIN interface / policy",L"BIN compatibility / LIVE policy"};
    const auto file_border=firmware_upgrade::visual::SourceColors(firmware_upgrade::visual::Provenance::ImageFile).border;
    for (int i=0;i<3;++i) Group(dc,layout.metadata_groups[i],titles[i],file_border);
    if (previous) dc.SelectObject(previous);
}

BOOL CMainFrame::OnEraseBkgnd(CDC* dc) {
    if (dc == nullptr) return FALSE;
    CRect rectangle; GetClientRect(&rectangle);
    dc->FillSolidRect(rectangle, GetSysColor(COLOR_3DFACE));
    return TRUE;
}
HBRUSH CMainFrame::OnCtlColor(CDC* dc, CWnd* window, UINT control_type) {
    HBRUSH brush = CFrameWnd::OnCtlColor(dc, window, control_type);
    if (dc == nullptr || window == nullptr ||
        page_background_brush_.GetSafeHandle() == nullptr) return brush;
    const HWND handle = window->GetSafeHwnd();
    if (control_type == CTLCOLOR_STATIC) {
        dc->SetTextColor(GetSysColor(COLOR_BTNTEXT));
        dc->SetBkColor(GetSysColor(COLOR_3DFACE));
        brush = static_cast<HBRUSH>(page_background_brush_.GetSafeHandle());
        for (auto& value:firmware_field_values_) if (handle==value.GetSafeHwnd()) {
            dc->SetTextColor(GetSysColor(COLOR_WINDOWTEXT)); dc->SetBkColor(GetSysColor(COLOR_WINDOW));
            brush=static_cast<HBRUSH>(field_background_brush_.GetSafeHandle()); break;
        }
    }
    const bool status_field =
        handle == firmware_field_values_[kFirmwareFieldChecksumResult].GetSafeHwnd() ||
        handle == firmware_field_values_[kFirmwareFieldMetadataStatus].GetSafeHwnd();
    if(control_type==CTLCOLOR_STATIC &&
        (handle==firmware_field_values_[kFirmwareFieldVersionControl].GetSafeHwnd() ||
         handle==firmware_field_values_[kFirmwareFieldInstalledVersionPolicy].GetSafeHwnd())) {
        CString text; window->GetWindowTextW(text);
        const auto tone=text.Find(L"ENABLED")==0 ? firmware_upgrade::visual::Tone::Pending :
            text.Find(L"DISABLED")==0 ? firmware_upgrade::visual::Tone::Ready : firmware_upgrade::visual::Tone::Neutral;
        const auto palette=firmware_upgrade::visual::Colors(tone);
        dc->SetTextColor(palette.text); dc->SetBkColor(palette.background);
        return static_cast<HBRUSH>((tone==firmware_upgrade::visual::Tone::Pending ? pending_background_brush_ :
            tone==firmware_upgrade::visual::Tone::Ready ? ready_background_brush_ : neutral_background_brush_).GetSafeHandle());
    }
    const bool policy_field=handle==firmware_field_values_[kFirmwareFieldUpdatePolicy].GetSafeHwnd() ||
        handle==firmware_field_values_[kFirmwareFieldBootAbi].GetSafeHwnd();
    if (policy_field && binary_data_.size()>=0x200U) {
        firmware_upgrade::image_policy::Policy policy;
        const bool valid=firmware_upgrade::image_policy::Parse(binary_data_.data()+0x100U,&policy);
        if (valid) {
            const bool compatible=firmware_upgrade::image_policy::Allows(policy,
                IsMiscTab()?IP_ROLE_BRIDGE_APP:IP_ROLE_DEVICE_APP,
                IsMiscTab()||IsUartTab()?IP_UART:(IsI2cTab()?IP_I2C:IP_CAN),nullptr);
            const auto palette=firmware_upgrade::visual::Colors(compatible?
                firmware_upgrade::visual::Tone::Ready:firmware_upgrade::visual::Tone::Error);
            dc->SetTextColor(palette.text); dc->SetBkColor(palette.background);
            return static_cast<HBRUSH>((compatible?ready_background_brush_:error_background_brush_).GetSafeHandle());
        }
    }
    if (status_field) {
        CString text; window->GetWindowTextW(text);
        const auto tone = text.Find(L"MATCH") == 0 || text == L"VALID" ?
            firmware_upgrade::visual::Tone::Ready :
            (text == L"N/A" ? firmware_upgrade::visual::Tone::Neutral :
                (text.Find(L"RAW BIN") == 0 ? firmware_upgrade::visual::Tone::Pending :
                                             firmware_upgrade::visual::Tone::Error));
        const auto palette = firmware_upgrade::visual::Colors(tone);
        dc->SetTextColor(palette.text); dc->SetBkColor(palette.background);
        return static_cast<HBRUSH>((tone == firmware_upgrade::visual::Tone::Ready ?
            ready_background_brush_ :
            (tone == firmware_upgrade::visual::Tone::Error ? error_background_brush_ :
                (tone == firmware_upgrade::visual::Tone::Pending ? pending_background_brush_ :
                                                       neutral_background_brush_))).GetSafeHandle());
    }
    return brush;
}
