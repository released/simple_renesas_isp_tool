#include "main_frame.h"
#include "ui/console_geometry.h"
#include <algorithm>

namespace {
using firmware_upgrade::console::Rect;
void Move(CWnd& control, const Rect& r) {
    if (control.GetSafeHwnd() != nullptr)
        control.MoveWindow(r.x, r.y, (std::max)(0, r.width), (std::max)(0, r.height), FALSE);
}
int CaptionWidth(CWnd& control, int minimum = 0, const wchar_t* caption = nullptr) {
    if (control.GetSafeHwnd() == nullptr) return minimum;
    CString text;
    if (caption) text = caption;
    else control.GetWindowTextW(text);
    CClientDC dc(&control);
    CFont* old = control.GetFont() ? dc.SelectObject(control.GetFont()) : nullptr;
    const int measured = dc.GetTextExtent(text).cx + 12;
    if (old) dc.SelectObject(old);
    return (std::max)(minimum, measured);
}
}
void CMainFrame::LayoutControls(int width, int height) {
    if (GetSafeHwnd() == nullptr || layout_in_progress_ || width <= 0 || height <= 0 ||
        detailed_trace_check_.GetSafeHwnd() == nullptr || bus_tabs_.GetSafeHwnd() == nullptr) return;
    layout_in_progress_ = true;
    active_tab_=bus_tabs_.GetCurSel();
    CClientDC dc(this);
    CFont* previous = dc.SelectObject(&ui_font_);
    TEXTMETRIC metric = {}; dc.GetTextMetrics(&metric);
    if (previous) dc.SelectObject(previous);
    const auto layout = firmware_upgrade::console::Calculate(width, height, metric.tmHeight);
    const int row = layout.field_row_height, gap = 5;
    int x = layout.connection.x;
    const int top = layout.connection.y;
    const auto pair = [&](CWnd& label, CWnd& value, int value_width) {
        const int label_width = CaptionWidth(label);
        Move(label, {x, top + 3, label_width, row});
        x += label_width + 3; Move(value, {x, top, value_width, 220});
        x += value_width + gap;
    };
    pair(com_label_, com_combo_, 285);
    com_combo_.SetDroppedWidth((std::max)(420, width - 100));
    pair(baud_label_, baud_combo_, 100);
    CButton* connection_buttons[] = {&refresh_com_btn_, &open_com_btn_, &close_com_btn_, &reset_host_btn_};
    for (auto* button : connection_buttons) {
        const int w = CaptionWidth(*button) + 12;
        Move(*button, {x, top, w, row}); x += w + gap;
    }
    const int half = (layout.identities.width - gap) / 2;
    Move(host_info_label_, {layout.identities.x, layout.identities.y, half, layout.identities.height});
    Move(target_info_label_, {layout.identities.x + half + gap, layout.identities.y, half, layout.identities.height});
    const int third = (layout.firmware.width - gap * 2) / 3;
    for (std::size_t i = 0; i < firmware_overview_cards_.size(); ++i)
        Move(firmware_overview_cards_[i], {layout.firmware.x + static_cast<int>(i) * (third + gap),
            layout.firmware.y, third, layout.firmware.height});
    Move(target_metadata_label_, layout.device_details);
    Move(operation_status_card_, layout.status);
    Move(interface_heading_, layout.interface_heading);
    Move(image_heading_, layout.image_heading);
    Move(log_heading_, layout.log_heading);
    Move(bus_tabs_, layout.tabs);
    CFont* tab_font = dc.SelectObject(&ui_font_);
    const int tab_caption = (std::max)({dc.GetTextExtent(L"CAN").cx,
        dc.GetTextExtent(L"I2C").cx, dc.GetTextExtent(L"Misc").cx}) + 24;
    if (tab_font) dc.SelectObject(tab_font);
    bus_tabs_.SetItemSize(CSize((std::max)(tab_caption, (layout.tabs.width - 8) / 3), row));
    LayoutInterfaceControls(layout.interface_pane.x, layout.interface_pane.y,
                            layout.interface_pane.width, layout.interface_pane.height, row);

    // These HWNDs are owned once by the common console, never by a bus TAB.
    const Rect image = layout.image_pane;
    const int load_width = CaptionWidth(load_binary_btn_) + 12;
    Move(load_binary_btn_, {image.x, image.y, load_width, row});
    Move(file_path_label_, {image.x + load_width + gap, image.y + 3,
        image.width - load_width - gap, row});
    const int address_y = image.y + row + gap;
    const int address_width = CaptionWidth(app_start_label_);
    Move(app_start_label_, {image.x, address_y + 3, address_width, row});
    Move(app_start_edit_, {image.x + address_width + gap, address_y, 118, row});
    int label_width = 0;
    for (auto& label : firmware_field_labels_) label_width = (std::max)(label_width, CaptionWidth(label));
    const int cell_width = (image.width - gap) / 2;
    label_width = (std::min)(label_width, cell_width * 46 / 100);
    const int fields_y = address_y + row + gap;
    for (std::size_t i = 0; i < kFirmwareFieldCount; ++i) {
        const int cx = image.x + static_cast<int>(i % 2) * (cell_width + gap);
        const int cy = fields_y + static_cast<int>(i / 2) * row;
        Move(firmware_field_labels_[i], {cx, cy + 3, label_width, row - 2});
        Move(firmware_field_values_[i], {cx + label_width + 3, cy,
            cell_width - label_width - 3, row - 2});
    }
    const int action_y = fields_y + row * static_cast<int>((kFirmwareFieldCount+1U)/2U) + gap;
    const int start_width = CaptionWidth(start_firmware_update_btn_) + 16;
    Move(start_firmware_update_btn_, {image.x, action_y, start_width, row + 5});
    Move(firmware_prerequisites_label_, {image.x + start_width + gap, action_y,
        image.width - start_width - gap, (std::max)(row + 8, image.y + image.height - action_y)});

    const Rect log = layout.log_pane;
    CButton* log_buttons[] = {&clear_log_btn_, &start_log_btn_, &stop_log_btn_, &export_summary_btn_};
    for (int i = 0; i < 4; ++i)
        Move(*log_buttons[i], {log.x + (i % 2) * ((log.width - gap) / 2 + gap),
            log.y + (i / 2) * (row + 2), (log.width - gap) / 2, row});
    const int trace_y = log.y + (row + 2) * 2;
    Move(detailed_trace_check_, {log.x, trace_y, log.width, row});
    const int log_y = trace_y + row + 2;
    Move(log_edit_, {log.x, log_y, log.width, (std::max)(0, log.y + log.height - log_y)});

    // Reserve 100% up front so progress updates never move the bar per packet.
    const std::wstring progress_sample = progress_operation_ + L": 100%";
    const auto footer = firmware_upgrade::console::CalculateFooter(layout.footer,
        CaptionWidth(progress_label_, 145, progress_sample.c_str()),
        CaptionWidth(operation_timer_label_, 145), CaptionWidth(build_info_label_, 270));
    Move(progress_label_, footer.progress_label);
    Move(progress_bar_, footer.progress_bar);
    Move(operation_timer_label_, footer.elapsed);
    Move(build_info_label_, footer.build);
    layout_in_progress_ = false;
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}
void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* info) {
    CFrameWnd::OnGetMinMaxInfo(info);
    info->ptMinTrackSize.x = 1260;
    info->ptMinTrackSize.y = 660;
}
