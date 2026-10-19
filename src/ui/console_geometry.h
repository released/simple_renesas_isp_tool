#pragma once

namespace firmware_upgrade::console {
struct Rect { int x, y, width, height; };
struct Layout {
    Rect connection_group, analysis_group, interface_group, image_group, log_group;
    Rect metadata_groups[4], metadata_panes[4];
    Rect connection, identities, firmware, device_details, status, tabs;
    Rect interface_heading, image_heading, log_heading;
    Rect interface_pane, image_pane, log_pane, footer;
    int field_row_height, metadata_row_height, interface_row_height;
    bool fits;
};
struct FooterLayout {
    Rect progress_label, progress_bar, elapsed, build;
    bool fits;
};
struct LogLayout { Rect buttons[6], trace, auto_scroll, history; };
LogLayout CalculateLog(const Rect& bounds,int row);
// All page bodies share these rectangles. Only log history scrolls; there is
// no document-sized parent surface. Metrics are actual device pixels.
Layout Calculate(int width, int height, int text_height);
int MinimumClientHeight(int text_height);
// Text widths include font-aware padding. Keep all children inside the band.
FooterLayout CalculateFooter(const Rect& bounds, int progress_width,
                             int elapsed_width, int build_width);
} // namespace firmware_upgrade::console
