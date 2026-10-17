#pragma once
#include <windows.h>
#include <climits>
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
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {if(error) *error=L"Report open failed (Windows "+std::to_wstring(GetLastError())+L").";return false;}
    DWORD written=0;
    const BOOL ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);
    DWORD code=GetLastError();
    const BOOL flushed=ok && written==bytes.size() && FlushFileBuffers(file);
    if(!flushed && ok) code=written==bytes.size()?GetLastError():ERROR_WRITE_FAULT;
    CloseHandle(file);
    if(!flushed) {if(error) *error=L"Report write/flush failed (Windows "+std::to_wstring(code)+L").";return false;}
    if(error) error->clear();
    return true;
}
} // namespace firmware_upgrade::evidence
