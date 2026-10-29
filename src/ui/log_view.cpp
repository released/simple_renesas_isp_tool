#include "log_view.h"
#include <richedit.h>

namespace firmware_upgrade::log_view {
void ScrollToEnd(HWND window) {
    if (!IsWindow(window)) return;
    CHARRANGE end={-1,-1};
    SendMessageW(window,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&end));
    SendMessageW(window,EM_SCROLLCARET,0,0);
    SendMessageW(window,WM_VSCROLL,SB_BOTTOM,0);
}
void Append(HWND window,const std::wstring& text,COLORREF color,bool follow) {
    AppendBatch(window,{{text,color}},follow);
}
void AppendBatch(HWND window,const std::vector<Line>& lines,bool follow) {
    if (!IsWindow(window) || lines.empty()) return;
    CHARRANGE selection={}; POINT viewport={};
    const auto first_line=SendMessageW(window,EM_GETFIRSTVISIBLELINE,0,0);
    SendMessageW(window,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&selection));
    const bool pixel_position=SendMessageW(window,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&viewport))!=0;
    const bool visible=(GetWindowLongPtrW(window,GWL_STYLE)&WS_VISIBLE)!=0;
    if (visible) SendMessageW(window,WM_SETREDRAW,FALSE,0);
    CHARRANGE end={-1,-1};
    SendMessageW(window,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&end));
    CHARFORMATW format={}; format.cbSize=sizeof(format);
    format.dwMask=CFM_COLOR;
    for(std::size_t i=0;i<lines.size();) {
        format.crTextColor=lines[i].color;
        std::wstring text=lines[i++].text;
        while(i<lines.size() && lines[i].color==format.crTextColor) text+=lines[i++].text;
        SendMessageW(window,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));
        SendMessageW(window,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(text.c_str()));
    }
    if (follow) {
        if(visible) SendMessageW(window,WM_SETREDRAW,TRUE,0);
        ScrollToEnd(window);
        if(visible) InvalidateRect(window,nullptr,FALSE);
    }
    else {
        SendMessageW(window,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));
        // Let RichEdit recalculate its scroll range before restoring the view.
        if (visible) SendMessageW(window,WM_SETREDRAW,TRUE,0);
        if (pixel_position) SendMessageW(window,EM_SETSCROLLPOS,0,reinterpret_cast<LPARAM>(&viewport));
        // Pixel scroll coordinates are 16-bit on RichEdit. Correct by line for
        // long histories; a second pass handles the control's line rounding.
        for (int attempt=0;attempt<3;++attempt) {
            const auto current_line=SendMessageW(window,EM_GETFIRSTVISIBLELINE,0,0);
            if (current_line==first_line) break;
            SendMessageW(window,EM_LINESCROLL,0,first_line-current_line);
        }
        if (visible) {
            InvalidateRect(window,nullptr,FALSE);
        }
    }
}
} // namespace firmware_upgrade::log_view
