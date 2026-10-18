#pragma once
#include <windows.h>
#include <climits>
#include <atomic>
#include <string>

namespace firmware_upgrade::evidence {
inline bool WriteReport(const std::wstring& path,const std::wstring& document,std::wstring* error) {
    if(document.empty() || document.size()>INT_MAX) {
        if(error) *error=L"Invalid report length."; return false;
    }
    const int length=static_cast<int>(document.size());
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,document.data(),length,nullptr,0,nullptr,nullptr);
    if(size<=0) {if(error) *error=L"UTF-8 report encoding failed.";return false;}
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,document.data(),length,bytes.data(),size,nullptr,nullptr)) return false;
    const auto partial=path+L".partial";
    HANDLE file=CreateFileW(partial.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {if(error) *error=L"Report open failed (Windows "+std::to_wstring(GetLastError())+L").";return false;}
    DWORD written=0;
    const BOOL ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);
    DWORD code=GetLastError();
    const BOOL flushed=ok && written==bytes.size() && FlushFileBuffers(file);
    if(!flushed && ok) code=written==bytes.size()?GetLastError():ERROR_WRITE_FAULT;
    CloseHandle(file);
    if(!flushed) {
        DeleteFileW(partial.c_str());
        if(error) *error=L"Report write/flush failed (Windows "+std::to_wstring(code)+L").";return false;
    }
    if(!MoveFileExW(partial.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) {
        code=GetLastError(); DeleteFileW(partial.c_str());
        if(error) *error=L"Report publish failed (Windows "+std::to_wstring(code)+L").";return false;
    }
    if(error) error->clear();
    return true;
}
struct SavedReport { std::wstring json_path, html_path, error; };
inline SavedReport SaveReportPair(const std::wstring& json,const std::wstring& html) {
    SavedReport saved;
    wchar_t module[MAX_PATH]={}; const DWORD n=GetModuleFileNameW(nullptr,module,MAX_PATH);
    if(!n || n>=MAX_PATH) {saved.error=L"Automatic report path resolution failed.";return saved;}
    std::wstring directory=module;
    directory=directory.substr(0,directory.find_last_of(L"\\/"))+L"\\reports";
    if(!CreateDirectoryW(directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) {
        saved.error=L"Report directory failed: "+directory+L" (Windows "+std::to_wstring(GetLastError())+L").";return saved;
    }
    SYSTEMTIME t={}; GetSystemTime(&t);
    static std::atomic<unsigned long long> sequence{0U};
    wchar_t stem[140]={}; swprintf_s(stem,L"\\update_%04u%02u%02u_%02u%02u%02u_%03u_%lu_%llu_%llu",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,
        GetCurrentProcessId(),GetTickCount64(),++sequence);
    const auto base=directory+stem;
    std::wstring error;
    if(WriteReport(base+L".json",json,&error)) saved.json_path=base+L".json";
    else saved.error=L"JSON: "+error;
    if(WriteReport(base+L".html",html,&error)) saved.html_path=base+L".html";
    else saved.error+=(saved.error.empty()?L"":L"; ")+std::wstring(L"HTML: ")+error;
    return saved;
}
} // namespace firmware_upgrade::evidence
