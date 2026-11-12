#include "main_frame.h"
#include "ui/console_geometry.h"
#include <algorithm>

namespace {
using firmware_upgrade::console::Rect;
void Move(CWnd& control, const Rect& r) {
    if (control.GetSafeHwnd() == nullptr) return;
    const CRect wanted(r.x, r.y, r.x + (std::max)(0, r.width), r.y + (std::max)(0, r.height));
    CRect current;
    control.GetWindowRect(&current);
    // A closed combo reports only its edit/button height. Compare the full
    // dropdown extent requested by MoveWindow, otherwise every state refresh
    // resizes it and sends expensive accessibility/layout notifications again.
    if (control.IsKindOf(RUNTIME_CLASS(CComboBox))) {
        static_cast<CComboBox&>(control).GetDroppedControlRect(&current);
    }
    if (auto* parent = control.GetParent()) parent->ScreenToClient(&current);
    // RichEdit reformats its entire history on WM_SIZE, even when the supplied
    // geometry is unchanged. State-only refreshes must not resize it again.
    if (current == wanted) return;
    control.MoveWindow(wanted, FALSE);
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
int HeaderInset(int width, int title_width, int build_width, int build_height, int text_height) {
    if (title_width + build_width + 40 > width) return build_height + 12;
    return (std::max)(0, build_height - text_height - 12);
}
}
void CMainFrame::MoveControl(CWnd& control, const firmware_upgrade::console::Rect& rectangle) {
    Move(control, rectangle);
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
    previous = dc.SelectObject(section_font_.GetSafeHandle() ? &section_font_ : &ui_font_);
    const int title_width = dc.GetTextExtent(firmware_upgrade::console::HostConnectionTitle).cx;
    dc.SelectObject(previous);
    build_identity_.SetFont(reinterpret_cast<HFONT>(ui_font_.GetSafeHandle()));
    const int build_height = build_identity_.Height();
    const int header_height = HeaderInset(width, title_width, build_identity_.PreferredWidth(), build_height, metric.tmHeight);
    const bool separate_header = header_height >= build_height;
    const int build_top = separate_header ? 6 : (std::max)(0, 6 - (build_height - static_cast<int>(metric.tmHeight)) / 2);
    build_identity_.Place(width - 12, build_top, (std::max)(0, width - 24 - (separate_header ? 0 : title_width + 16)));
    auto layout = firmware_upgrade::console::Calculate(width, (std::max)(0, height - header_height), metric.tmHeight,
        InterfaceRowHeight(metric.tmHeight));
    // Translate the existing console as one body; keep its internal geometry.
    Rect* regions[] = {&layout.connection_group, &layout.analysis_group, &layout.interface_group,
        &layout.image_group, &layout.log_group, &layout.connection, &layout.identities,
        &layout.firmware, &layout.loaded_bin, &layout.bin_details, &layout.device_details,
        &layout.status, &layout.tabs, &layout.interface_heading, &layout.image_heading,
        &layout.log_heading, &layout.interface_pane, &layout.image_pane, &layout.log_pane, &layout.footer};
    for (auto* region : regions) region->y += header_height;
    for (auto& region : layout.metadata_groups) region.y += header_height;
    for (auto& region : layout.metadata_panes) region.y += header_height;
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
    const bool show_force=(force_old_firmware_btn_.GetStyle() & WS_VISIBLE)!=0;
    const bool show_update=(start_firmware_update_btn_.GetStyle() & WS_VISIBLE)!=0;
    const bool force_inline=start_width+gap+force_width<=image.width;
    const int force_y=force_inline?action_y:action_y+row+gap+5;
    Move(force_old_firmware_btn_,{force_inline?image.x+start_width+gap:image.x,force_y,
        (std::min)(force_width,image.width),row+5});
    const int notice_y=show_update ? (show_force?force_y:action_y)+row+gap+5 : action_y;
    Move(firmware_prerequisites_label_, {image.x,notice_y,image.width,
        (std::max)(0,image.y+image.height-notice_y)});

    FitWorkflowHint();

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
    build_info_label_.ShowWindow(SW_HIDE);
    // Native combo sizing can select its current list item after UpdateUi.
    // Settle inactive edit selection after the final first-show/resize layout.
    ClearInactiveUartBaudSelection();
    layout_in_progress_ = false;
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}
void CMainFrame::FitWorkflowHint() {
    if (!firmware_prerequisites_label_.GetSafeHwnd() || !ui_font_.GetSafeHandle()) return;
    CRect bounds;
    firmware_prerequisites_label_.GetWindowRect(&bounds);
    ScreenToClient(&bounds);
    if (bounds.Width() <= 16) return;
    CClientDC dc(&firmware_prerequisites_label_);
    auto* previous = dc.SelectObject(section_font_.GetSafeHandle() ? &section_font_ : &ui_font_);
    TEXTMETRIC metric = {};
    dc.GetTextMetrics(&metric);
    CString text;
    firmware_prerequisites_label_.GetWindowTextW(text);
    CRect content(0, 0, bounds.Width()-16, 0);
    dc.DrawTextW(text, content, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    dc.SelectObject(previous);
    const int available = (std::max)(0, console_layout_.image_pane.y+console_layout_.image_pane.height-static_cast<int>(bounds.top));
    const int wanted = (std::min)(available, (std::max)(static_cast<int>(metric.tmHeight)*3+8, content.Height()+8));
    if (bounds.Height() == wanted) return;
    const CRect exposed = bounds;
    bounds.bottom = bounds.top+wanted;
    firmware_prerequisites_label_.MoveWindow(bounds, FALSE);
    InvalidateRect(exposed, TRUE);
    firmware_prerequisites_label_.Invalidate(FALSE);
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* info) {
    CFrameWnd::OnGetMinMaxInfo(info);
    info->ptMinTrackSize.x = 1260;
    info->ptMinTrackSize.y = 700;
    if (GetSafeHwnd() && ui_font_.GetSafeHandle()) {
        CClientDC dc(this); auto* previous=dc.SelectObject(&ui_font_);
        TEXTMETRIC metric={}; dc.GetTextMetrics(&metric); dc.SelectObject(previous);
        previous=dc.SelectObject(section_font_.GetSafeHandle() ? &section_font_ : &ui_font_);
        const int title_width=dc.GetTextExtent(firmware_upgrade::console::HostConnectionTitle).cx;
        dc.SelectObject(previous);
        CRect window,client; GetWindowRect(&window); GetClientRect(&client);
        const int header_height=HeaderInset(info->ptMinTrackSize.x-window.Width()+client.Width(),
            title_width,build_identity_.PreferredWidth(),build_identity_.Height(),metric.tmHeight);
        const int native_row=InterfaceRowHeight(metric.tmHeight);
        int required_interface_height=0;
        if(i2c_notes_.GetSafeHwnd() && (IsI2cTab() || IsUartTab() || IsMiscTab())) {
            const auto minimum_layout=firmware_upgrade::console::Calculate(
                info->ptMinTrackSize.x-window.Width()+client.Width(),
                firmware_upgrade::console::MinimumClientHeight(metric.tmHeight,native_row),metric.tmHeight,native_row);
            CString reminder;i2c_notes_.GetWindowTextW(reminder);
            CRect measured(0,0,(std::max)(1,minimum_layout.interface_pane.width-16),0);
            previous=dc.SelectObject(&ui_font_);
            dc.DrawTextW(reminder,measured,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
            dc.SelectObject(previous);
            const int note_row=IsMiscTab()?4:IsI2cIsp()?8:7;
            required_interface_height=note_row*(native_row+firmware_upgrade::console::InterfaceRowGap)+measured.Height()+6;
        }
        info->ptMinTrackSize.y=(std::max)(700,
            firmware_upgrade::console::MinimumClientHeight(metric.tmHeight,native_row,required_interface_height)+
            header_height+window.Height()-client.Height());
    }
}

void CMainFrame::SetProgress(int value, const std::wstring& label, bool indeterminate) {
    const int clamped = (std::max)(0, (std::min)(value, 100));
    const bool mode_changed=progress_indeterminate_!=indeterminate;
    if(mode_changed) {
        if(indeterminate) {
            progress_bar_.ModifyStyle(0,PBS_MARQUEE);
            progress_bar_.SetMarquee(TRUE,40U);
        } else {
            progress_bar_.SetMarquee(FALSE,0U);
            progress_bar_.ModifyStyle(PBS_MARQUEE,0);
        }
        progress_indeterminate_=indeterminate;
    }
    if(!indeterminate) progress_bar_.SetPos(clamped);
    progress_label_.SetWindowTextW(
        (label + (indeterminate ? L": waiting..." : L": "+std::to_wstring(clamped)+L"%")).c_str());
    if (progress_operation_ != label || mode_changed) {
        progress_operation_ = label;
        // Only the footer depends on the operation caption. Keep the rest of
        // the console, especially long RichEdit history, out of this refresh.
        const auto footer = firmware_upgrade::console::CalculateFooter(console_layout_.footer,
            CaptionWidth(progress_label_, 145, (progress_operation_ + L": waiting...").c_str()),
            CaptionWidth(operation_timer_label_, 145), CaptionWidth(build_info_label_, 270));
        Move(progress_label_, footer.progress_label);
        Move(progress_bar_, footer.progress_bar);
        Move(operation_timer_label_, footer.elapsed);
        Move(build_info_label_, footer.build);
    build_info_label_.ShowWindow(SW_HIDE);
        const auto& area = console_layout_.footer;
        CRect band(area.x, area.y, area.x + area.width, area.y + area.height);
        RedrawWindow(&band, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}
