#include "console_geometry.h"
#include <algorithm>

namespace firmware_upgrade::console {
Layout Calculate(int width, int height, int text_height) {
    const int margin = 6, gap = 5;
    const int row = (std::max)(24, text_height + 8);
    const int card = text_height * 4 + 12;
    const int content_width = (std::max)(0, width - margin * 2);
    int y = margin;
    Layout out = {};
    const auto band = [&](int h) {
        const Rect r = {margin, y, content_width, h}; y += h + gap; return r;
    };
    out.connection = band(row + 2);
    if (width<1600 || height<780) {
        const Rect cards=band(card);
        const int identity_width=(content_width-gap*4)*2/5+gap;
        out.identities={cards.x,cards.y,identity_width,card};
        out.firmware={cards.x+identity_width+gap,cards.y,content_width-identity_width-gap,card};
    } else {
        out.identities = band(card);
        out.firmware = band(card);
    }
    out.device_details = band(text_height * 2 + 4);
    out.status = band(text_height + 8);
    const int footer_height = row;
    const int body_height = (std::max)(0, height - y - footer_height - gap - margin);
    const int interface_width = (std::max)(238, content_width * 19 / 100);
    const int log_width = (std::max)(300, content_width * 24 / 100);
    const int image_width = (std::max)(0, content_width - interface_width - log_width - gap * 2);
    const int heading_height = row + 3;
    const int content_y = y + heading_height + gap;
    const int content_height = (std::max)(0, body_height - heading_height - gap);
    const int image_x = margin + interface_width + gap;
    const int log_x = image_x + image_width + gap;
    out.interface_heading = {margin, y, interface_width, heading_height};
    out.image_heading = {image_x, y, image_width, heading_height};
    out.log_heading = {log_x, y, log_width, heading_height};
    // The TAB strip belongs only to the left settings column. Shared image
    // and log start independently below their own persistent headings.
    out.tabs = {margin, content_y, interface_width, row + 3};
    out.interface_pane = {margin, content_y + out.tabs.height + gap, interface_width,
        (std::max)(0, content_height - out.tabs.height - gap)};
    out.image_pane = {image_x, content_y, image_width, content_height};
    out.log_pane = {log_x, content_y, log_width, content_height};
    out.footer = {margin, y + body_height + gap, content_width, footer_height};
    out.field_row_height = row;
    out.fits = image_width >= 620 && content_height >= row * 14 + 23 &&
        out.interface_pane.height >= (row + 2) * 10 + row;
    return out;
}
FooterLayout CalculateFooter(const Rect& bounds, int progress_width,
                             int elapsed_width, int build_width) {
    const int gap = 6, minimum_bar = 120;
    const int width = (std::max)(0, bounds.width);
    const int height = (std::max)(0, bounds.height);
    // Under unsupported narrow widths, shrink text rather than overlap HWNDs.
    const int spacing = (std::min)(gap, width / 3);
    int available = width - spacing * 3;
    const int bar_min = (std::min)(minimum_bar, available);
    int text_budget = available - bar_min;
    const int elapsed = (std::min)((std::max)(0, elapsed_width), text_budget);
    text_budget -= elapsed;
    const int progress = (std::min)((std::max)(0, progress_width), text_budget);
    text_budget -= progress;
    const int build = (std::min)((std::max)(0, build_width), text_budget);
    const int bar = available - elapsed - progress - build;
    FooterLayout out = {};
    out.progress_label = {bounds.x, bounds.y, progress, height};
    out.progress_bar = {bounds.x + progress + spacing, bounds.y + (height > 2 ? 1 : 0),
                        bar, (std::max)(0, height - 2)};
    out.elapsed = {out.progress_bar.x + bar + spacing, bounds.y, elapsed, height};
    out.build = {out.elapsed.x + elapsed + spacing, bounds.y, build, height};
    out.fits = progress >= progress_width && elapsed >= elapsed_width &&
        build >= build_width && bar >= minimum_bar && height > 2;
    return out;
}
} // namespace firmware_upgrade::console
