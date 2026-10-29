#include "../src/ui/log_view.h"
#include <richedit.h>
#include <iostream>

int TestLogView() {
    using namespace firmware_upgrade;
    int failures=0;
    const auto check=[&](bool ok,const char* name) {if(!ok) {++failures; std::cerr<<"FAIL log view: "<<name<<'\n';}};
    HMODULE rich=LoadLibraryW(L"Riched20.dll");
    check(rich!=nullptr,"RichEdit loaded"); if(!rich) return failures;
    HWND parent=CreateWindowExW(0,L"STATIC",L"Log test",WS_POPUP,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND log=CreateWindowExW(0,L"RichEdit20W",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,
        0,0,400,180,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(log!=nullptr,"real RichEdit created");
    if(log) {
        SendMessageW(log,EM_EXLIMITTEXT,0,8U*1024U*1024U);
        for(int i=0;i<350;++i) log_view::Append(log,L"message "+std::to_wstring(i)+L"\r\n",RGB(0,0,0),true);
        POINT bottom={}; SendMessageW(log,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&bottom));
        check(bottom.y>100,"enabled follows actual latest message");
        CHARRANGE selected={12,22}; SendMessageW(log,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selected));
        POINT requested={0,120}; SendMessageW(log,EM_SETSCROLLPOS,0,reinterpret_cast<LPARAM>(&requested));
        POINT before={}; SendMessageW(log,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&before));
        const int length=GetWindowTextLengthW(log);
        for(int i=0;i<100;++i) log_view::Append(log,L"new message while reading history\r\n",RGB(196,24,24),false);
        POINT after={}; CHARRANGE restored={};
        SendMessageW(log,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&after));
        SendMessageW(log,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&restored));
        check(after.x==before.x && after.y==before.y,"disabled preserves pixel viewport across100 appends");
        check(restored.cpMin==selected.cpMin && restored.cpMax==selected.cpMax,"disabled preserves selected text");
        check(GetWindowTextLengthW(log)>length,"disabled still appends new messages");
        log_view::ScrollToEnd(log);
        SendMessageW(log,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&after));
        check(after.y>before.y,"re-enabling can return to bottom immediately");
        log_view::Append(log,L"final message\r\n",RGB(0,0,0),true);
        SendMessageW(log,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&restored));
        GETTEXTLENGTHEX count={GTL_PRECISE|GTL_NUMCHARS,1200};
        const auto rich_length=SendMessageW(log,EM_GETTEXTLENGTHEX,reinterpret_cast<WPARAM>(&count),0);
        check(restored.cpMin==rich_length,"enabled caret follows final appended text using RichEdit paragraph positions");
        std::wstring bulk;
        for(int i=0;i<15000;++i) bulk+=L"large log history line\r\n";
        log_view::Append(log,bulk,RGB(0,0,0),true);
        const auto first=SendMessageW(log,EM_GETFIRSTVISIBLELINE,0,0);
        check(first>5000,"large history exercises positions beyond the short viewport range");
        for(int i=0;i<100;++i) log_view::Append(log,L"arrived while paused\r\n",RGB(0,0,0),false);
        check(SendMessageW(log,EM_GETFIRSTVISIBLELINE,0,0)==first,"large paused history preserves visible line across100 appends");
        SendMessageW(log,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selected));
        SendMessageW(log,EM_LINESCROLL,0,-5000);
        const auto middle=SendMessageW(log,EM_GETFIRSTVISIBLELINE,0,0);
        for(int i=0;i<100;++i) log_view::Append(log,L"new wrapped message while selecting old text in a very long history\r\n",RGB(0,0,0),false);
        SendMessageW(log,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&restored));
        check(middle>1000 && SendMessageW(log,EM_GETFIRSTVISIBLELINE,0,0)==middle,"large middle viewport remains stable across100 wrapped appends");
        check(restored.cpMin==selected.cpMin && restored.cpMax==selected.cpMax,"large history preserves selection independently of viewport");
        const std::vector<log_view::Line> batch={{L"batch normal one\r\n",RGB(0,0,0)},
            {L"batch normal two\r\n",RGB(0,0,0)},{L"batch error\r\n",RGB(196,24,24)}};
        const auto paused=SendMessageW(log,EM_GETFIRSTVISIBLELINE,0,0);
        log_view::AppendBatch(log,batch,false);
        SendMessageW(log,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&restored));
        check(SendMessageW(log,EM_GETFIRSTVISIBLELINE,0,0)==paused && restored.cpMin==selected.cpMin && restored.cpMax==selected.cpMax,
            "mixed-color batch preserves paused viewport/selection");
        log_view::AppendBatch(log,batch,true);
        SendMessageW(log,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&restored));
        const auto batch_length=SendMessageW(log,EM_GETTEXTLENGTHEX,reinterpret_cast<WPARAM>(&count),0);
        check(restored.cpMin==batch_length,"batch follow ends at actual final RichEdit position");
        CHARRANGE last={static_cast<LONG>(batch_length)-2,static_cast<LONG>(batch_length)-1};
        SendMessageW(log,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&last));
        CHARFORMATW color={};color.cbSize=sizeof(color);
        SendMessageW(log,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&color));
        check(color.crTextColor==RGB(196,24,24),"batch preserves error text color");
        ShowWindow(log,SW_HIDE);
        log_view::AppendBatch(log,batch,true);
        check((GetWindowLongPtrW(log,GWL_STYLE)&WS_VISIBLE)==0,"batch never exposes a hidden control");
        DestroyWindow(log);
    }
    if(parent) DestroyWindow(parent); FreeLibrary(rich);
    log_view::Append(nullptr,L"no control",RGB(0,0,0),false);
    std::cout<<"Real RichEdit append / viewport / selection / follow tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
