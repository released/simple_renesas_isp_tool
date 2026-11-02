#pragma once
#include <windows.h>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <string>

namespace firmware_upgrade::text_file {
inline bool Fail(std::wstring* error, const std::wstring& message) {
    if (error) *error = message;
    return false;
}
inline bool Utf8(const std::wstring& text, std::string* bytes, std::wstring* error) {
    if (!bytes || text.size() > INT_MAX) return Fail(error, L"Invalid UTF-8 input length.");
    bytes->clear();
    if (text.empty()) return true;
    // An explicit length excludes NUL in both calls; capacity and return count agree.
    const int length = static_cast<int>(text.size());
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), length, nullptr, 0, nullptr, nullptr);
    if (!count) return Fail(error, L"UTF-8 size conversion failed (Windows " + std::to_wstring(GetLastError()) + L").");
    bytes->resize(static_cast<std::size_t>(count));
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
        bytes->data(), count, nullptr, nullptr) != count) {
        bytes->clear();
        return Fail(error, L"UTF-8 conversion failed (Windows " + std::to_wstring(GetLastError()) + L").");
    }
    return true;
}
inline bool Write(FILE* file, const std::string& bytes, std::wstring* error) {
    if (!file) return Fail(error, L"Text output is not open.");
    if (fwrite(bytes.data(), 1U, bytes.size(), file) != bytes.size() || ferror(file))
        return Fail(error, L"Text write failed (CRT " + std::to_wstring(errno) + L").");
    return true;
}
inline bool Flush(FILE* file, std::wstring* error) {
    if (!file || fflush(file) != 0)
        return Fail(error, L"Text flush failed (CRT " + std::to_wstring(errno) + L").");
    return true;
}
inline bool Close(FILE** owner, std::wstring* error) {
    if (!owner || !*owner) return true;
    FILE* file = *owner;
    *owner = nullptr;
    const bool flushed = Flush(file, error);
    const int closed = fclose(file);
    if (closed && flushed) return Fail(error, L"Text close failed (CRT " + std::to_wstring(errno) + L").");
    return flushed && closed == 0;
}
inline bool OpenExclusive(const std::wstring& path, FILE** owner, std::wstring* error) {
    if (!owner || *owner) return Fail(error, L"Invalid text file owner.");
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return Fail(error, L"Log create failed (Windows " + std::to_wstring(GetLastError()) + L"): " + path);
    const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_WRONLY | _O_BINARY);
    if (descriptor == -1) {
        CloseHandle(handle);
        return Fail(error, L"Log descriptor creation failed.");
    }
    *owner = _fdopen(descriptor, "wb");
    if (!*owner) {
        _close(descriptor);
        return Fail(error, L"Log stream creation failed.");
    }
    return true;
}
inline bool OpenLog(const std::wstring& directory, std::wstring* path, FILE** owner, std::wstring* error) {
    if (!path) return Fail(error, L"Log path output is missing.");
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    static std::atomic<unsigned long long> sequence{0U};
    wchar_t name[160] = {};
    swprintf_s(name, L"\\firmware_upgrade_tool_%04u%02u%02u_%02u%02u%02u_%03u_%lu_%llu.log",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, GetCurrentProcessId(), ++sequence);
    *path = directory + name;
    return OpenExclusive(*path, owner, error);
}
} // namespace firmware_upgrade::text_file
