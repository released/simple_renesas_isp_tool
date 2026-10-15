#include "main_frame.h"

BOOL CMainFrame::OnEraseBkgnd(CDC* dc) {
    if (dc == nullptr) return FALSE;
    CRect rectangle; GetClientRect(&rectangle);
    dc->FillSolidRect(rectangle, RGB(245,247,251));
    return TRUE;
}
HBRUSH CMainFrame::OnCtlColor(CDC* dc, CWnd* window, UINT control_type) {
    HBRUSH brush = CFrameWnd::OnCtlColor(dc, window, control_type);
    if (dc == nullptr || window == nullptr ||
        page_background_brush_.GetSafeHandle() == nullptr) return brush;
    const HWND handle = window->GetSafeHwnd();
    if (control_type == CTLCOLOR_STATIC) {
        dc->SetTextColor(RGB(36,51,71));
        dc->SetBkColor(RGB(245,247,251));
        brush = static_cast<HBRUSH>(page_background_brush_.GetSafeHandle());
    }
    const bool status_field =
        handle == firmware_field_values_[kFirmwareFieldChecksumResult].GetSafeHwnd() ||
        handle == firmware_field_values_[kFirmwareFieldMetadataStatus].GetSafeHwnd();
    const bool policy_field=handle==firmware_field_values_[kFirmwareFieldUpdatePolicy].GetSafeHwnd() ||
        handle==firmware_field_values_[kFirmwareFieldBootAbi].GetSafeHwnd();
    if (policy_field && binary_data_.size()>=0x200U) {
        firmware_upgrade::image_policy::Policy policy;
        const bool valid=firmware_upgrade::image_policy::Parse(binary_data_.data()+0x100U,&policy);
        if (valid) {
            const bool compatible=firmware_upgrade::image_policy::Allows(policy,
                IsMiscTab()?IP_ROLE_BRIDGE_APP:IP_ROLE_DEVICE_APP,
                IsMiscTab()?IP_UART:(IsI2cTab()?IP_I2C:IP_CAN),nullptr);
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
