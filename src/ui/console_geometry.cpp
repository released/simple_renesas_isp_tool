#include "console_geometry.h"
#include <algorithm>

namespace firmware_upgrade::console {
LogLayout CalculateLog(const Rect& bounds,int row) {
    LogLayout out={};
    const int gap=5, width=(std::max)(0,bounds.width-gap)/2;
    for(int i=0;i<6;++i) out.buttons[i]={bounds.x+(i%2)*(width+gap),bounds.y+(i/2)*(row+2),width,row};
    const int trace_y=bounds.y+(row+2)*3;
    out.trace={bounds.x,trace_y,bounds.width,row};
    out.auto_scroll={bounds.x,trace_y+row,bounds.width,row};
    const int history_y=trace_y+row*2+2;
    out.history={bounds.x,history_y,bounds.width,(std::max)(0,bounds.y+bounds.height-history_y)};
    return out;
}
Layout Calculate(int width, int height, int text_height) {
    const int margin = 6, gap = 5, padding = 6;
    const int row = (std::max)(24, text_height + 8);
    const int field_row=(std::max)(20,text_height+4);
    const int compact_row=(std::max)(22,text_height+4);
    const int title=text_height+8, card=text_height*3+10;
    const int content_width = (std::max)(0, width - margin * 2);
    int y = margin;
    Layout out = {};
    const auto band = [&](int h) {
        const Rect r = {margin, y, content_width, h}; y += h + gap; return r;
    };
    out.connection_group=band(title+row+padding);
    out.connection={margin+padding,out.connection_group.y+title,content_width-padding*2,row};
    const int metadata_height=title+field_row*5+padding;
    out.analysis_group=band(title+card+gap+metadata_height+padding);
    const int inner_width=content_width-padding*2;
    const int card_width=(inner_width-gap*4)/5;
    out.identities={margin+padding,out.analysis_group.y+title,card_width*2+gap,card};
    out.firmware={out.identities.x+out.identities.width+gap,out.identities.y,
        inner_width-out.identities.width-gap,card};
    const int group_y=out.identities.y+card+gap;
    const int metadata_width=(inner_width-gap*3)/4;
    for (int i=0;i<4;++i) {
        const int left=margin+padding+i*(metadata_width+gap);
        const int w=i==3 ? margin+content_width-padding-left : metadata_width;
        out.metadata_groups[i]={left,group_y,w,metadata_height};
        out.metadata_panes[i]={left+padding,group_y+title,w-padding*2,field_row*5};
    }
    out.status = band(text_height + 8);
    const int footer_height = row;
    const int body_height = (std::max)(0, height - y - footer_height - gap - margin);
    const int interface_width = (std::max)(270, content_width * 23 / 100);
    const int log_width = (std::max)(320, content_width * 29 / 100);
    const int image_width = (std::max)(0, content_width - interface_width - log_width - gap * 2);
    const int heading_height = title;
    const int content_y = y + heading_height;
    const int content_height = (std::max)(0, body_height - heading_height - padding);
    const int image_x = margin + interface_width + gap;
    const int log_x = image_x + image_width + gap;
    out.interface_heading = {margin, y, interface_width, heading_height};
    out.image_heading = {image_x, y, image_width, heading_height};
    out.log_heading = {log_x, y, log_width, heading_height};
    // The TAB strip belongs only to the left settings column. Shared image
    // and log start independently below their own persistent headings.
    out.interface_group={margin,y,interface_width,body_height};
    out.image_group={image_x,y,image_width,body_height};
    out.log_group={log_x,y,log_width,body_height};
    out.tabs = {margin+padding, content_y, interface_width-padding*2, compact_row+3};
    out.interface_pane = {margin+padding, content_y + out.tabs.height + gap, interface_width-padding*2,
        (std::max)(0, content_height - out.tabs.height - gap)};
    out.image_pane = {image_x+padding, content_y, image_width-padding*2, content_height};
    out.log_pane = {log_x+padding, content_y, log_width-padding*2, content_height};
    out.device_details={out.image_pane.x,content_y+(row+gap)*3,out.image_pane.width,text_height*2+4};
    out.footer = {margin, y + body_height + gap, content_width, footer_height};
    out.field_row_height = row;
    out.metadata_row_height=field_row;
    out.interface_row_height=compact_row;
    out.fits = image_width >= 400 && metadata_width>=285 &&
        out.interface_pane.height >= (compact_row+2)*9+compact_row &&
        content_height>=row*7;
    return out;
}
int MinimumClientHeight(int text_height) {
    const int row=(std::max)(24,text_height+8), compact=(std::max)(22,text_height+4);
    const int title=text_height+8, field=(std::max)(20,text_height+4);
    const int connection=title+row+6;
    const int analysis=title+(text_height*3+10)+5+(title+field*5+6)+6;
    const int top=6+connection+5+analysis+5+(text_height+8)+5;
    const int body=(std::max)({(compact+3)+5+(compact+2)*9+compact+title+6,
        (row+2)*3+row*2+2+80+title+6,row*7+title+6});
    return top+body+row+5+6;
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
