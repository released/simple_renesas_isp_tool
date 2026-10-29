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
    console_layout_=layout;
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
    CButton* connection_buttons[] = {&refresh_com_btn_, &open_com_btn_, &close_com_btn_, &reset_host_btn_, &cancel_btn_};
    for (auto* button : connection_buttons) {
        const int w = CaptionWidth(*button) + 12;
        Move(*button, {x, top, w, row}); x += w + gap;
    }
    const int half = (layout.identities.width - gap) / 2;
    Move(host_info_label_, {layout.identities.x, layout.identities.y, half, layout.identities.height});
    Move(target_info_label_, {layout.identities.x + half + gap, layout.identities.y, half, layout.identities.height});
    const int firmware_width = (layout.firmware.width - gap) / 2;
    for (std::size_t i = 0; i < 2U; ++i)
        Move(firmware_overview_cards_[i], {layout.firmware.x + static_cast<int>(i) * (firmware_width + gap),
            layout.firmware.y, firmware_width, layout.firmware.height});
    Move(firmware_overview_cards_[2],layout.loaded_bin);
    Move(bin_details_btn_,layout.bin_details);
    Move(target_metadata_label_, layout.device_details);
    Move(operation_status_card_, layout.status);
    Move(bus_tabs_, layout.tabs);
    // Natural caption widths keep the UART/Xmodem entry visible in the left
    // column without forcing four copies of the longest tab width.
    bus_tabs_.SetPadding(CSize(6,3));
    bus_tabs_.SetItemSize(CSize(0,layout.interface_row_height));
    LayoutInterfaceControls(layout.interface_pane.x, layout.interface_pane.y,
                            layout.interface_pane.width, layout.interface_pane.height, layout.interface_row_height);

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
    const int compare_x=image.x+address_width+gap+118+gap;
    const int compare_width=(std::min)(CaptionWidth(compare_metadata_btn_)+12,
        (std::max)(0,image.x+image.width-compare_x-CaptionWidth(preflight_btn_)-12-gap));
    Move(compare_metadata_btn_,{compare_x,address_y,compare_width,row});
    Move(preflight_btn_,{compare_x+compare_width+gap,address_y,
        (std::max)(0,image.x+image.width-compare_x-compare_width-gap),row});
    const FirmwareFieldIndex groups[3][4]={
        {kFirmwareFieldChecksumResult,kFirmwareFieldMetadataStatus,kFirmwareFieldAppRange,kFirmwareFieldImageCapacity},
        {kFirmwareFieldCanRates,kFirmwareFieldCanIds,kFirmwareFieldUpdatePolicy,kFirmwareFieldProtocolFormat},
        {kFirmwareFieldBootAbi,kFirmwareFieldVersionControl,kFirmwareFieldInstalledVersionPolicy,kFirmwareFieldFlashBoot}};
    for (std::size_t field=0;field<kFirmwareFieldCount;++field) {
        firmware_field_labels_[field].ShowWindow(SW_HIDE);
        firmware_field_values_[field].ShowWindow(SW_HIDE);
    }
    for (std::size_t group=0;group<3;++group) {
        const auto& pane=layout.metadata_panes[group];
        int label_width=0;
        for (auto field:groups[group]) label_width=(std::max)(label_width,CaptionWidth(firmware_field_labels_[field]));
        for (std::size_t item=0;item<4;++item) {
            const auto field=groups[group][item]; const int cy=pane.y+static_cast<int>(item)*layout.metadata_row_height;
            Move(firmware_field_labels_[field],{pane.x,cy+2,label_width,layout.metadata_row_height-2});
            Move(firmware_field_values_[field],{pane.x+label_width+3,cy,
                pane.width-label_width-3,layout.metadata_row_height-2});
            firmware_field_labels_[field].ShowWindow(SW_SHOW);
            firmware_field_values_[field].ShowWindow(SW_SHOW);
        }
    }
    const int action_y = layout.device_details.y+layout.device_details.height+gap;
    const int start_width = CaptionWidth(start_firmware_update_btn_) + 16;
    Move(start_firmware_update_btn_, {image.x, action_y, start_width, row + 5});
    const int force_width=CaptionWidth(force_old_firmware_btn_)+16;
    const bool show_force=!IsMiscTab() && (!IsI2cTab()||IsI2cIsp());
    const bool force_inline=start_width+gap+force_width<=image.width;
    const int force_y=force_inline?action_y:action_y+row+gap+5;
    Move(force_old_firmware_btn_,{force_inline?image.x+start_width+gap:image.x,force_y,
        (std::min)(force_width,image.width),row+5});
    const int notice_y=(show_force?force_y:action_y)+row+gap+5;
    Move(firmware_prerequisites_label_, {image.x,notice_y,image.width,
        (std::max)(0,image.y+image.height-notice_y)});

    const Rect log = layout.log_pane;
    const auto log_tools=firmware_upgrade::console::CalculateLog(log,row);
    CButton* log_buttons[] = {&clear_log_btn_, &export_summary_btn_, &start_log_btn_, &stop_log_btn_, &open_report_btn_, &error_details_btn_};
    for (int i = 0; i < 6; ++i)
        Move(*log_buttons[i],log_tools.buttons[i]);
    Move(detailed_trace_check_,log_tools.trace);
    Move(log_auto_scroll_check_,log_tools.auto_scroll);
    Move(log_edit_,log_tools.history);

    // Reserve the complete waiting caption as well as the 100-percent suffix.
    const std::wstring progress_sample = progress_operation_ + L": waiting...";
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
    info->ptMinTrackSize.y = 700;
    if (GetSafeHwnd() && ui_font_.GetSafeHandle()) {
        CClientDC dc(this); auto* previous=dc.SelectObject(&ui_font_);
        TEXTMETRIC metric={}; dc.GetTextMetrics(&metric); dc.SelectObject(previous);
        CRect window,client; GetWindowRect(&window); GetClientRect(&client);
        info->ptMinTrackSize.y=(std::max)(700,
            firmware_upgrade::console::MinimumClientHeight(metric.tmHeight)+window.Height()-client.Height());
    }
}
