#pragma once
#include <windows.h>
#include <string>

namespace firmware_upgrade::application_instance {
inline constexpr wchar_t kIdentity[] = L"SimpleRenesasIspTool.Owner.8D41F470.v1";
inline UINT kActivateMessage = RegisterWindowMessageW(L"SimpleRenesasIspTool.Activate.8D41F470.v1");

enum class StartResult { Owner, Existing, Failed };

// OS lifetime ownership is stable across paths, names and build configurations.
// Private identities are available to native fixtures, not a production switch.
class Guard {
public:
    explicit Guard(const std::wstring& identity = kIdentity) : identity_(identity) {}
    ~Guard() { Release(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    StartResult Acquire(std::wstring* error) {
        if (mutex_) {
            return owned_ ? StartResult::Owner : StartResult::Existing;
        }
        const auto name = L"Local\\" + identity_;
        mutex_ = CreateMutexW(nullptr, TRUE, name.c_str());
        const DWORD code = GetLastError();
        if (!mutex_) {
            if (error) {
                *error = L"Application ownership failed (Windows " + std::to_wstring(code) + L").";
            }
            return StartResult::Failed;
        }
        owned_ = code != ERROR_ALREADY_EXISTS;
        if (!owned_) {
            const DWORD wait = WaitForSingleObject(mutex_, 0U);
            if (wait == WAIT_FAILED) {
                if (error) {
                    *error = L"Application ownership wait failed (Windows " + std::to_wstring(GetLastError()) + L").";
                }
                Release();
                return StartResult::Failed;
            }
            owned_ = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        }
        return owned_ ? StartResult::Owner : StartResult::Existing;
    }
    bool Attach(HWND window) {
        if (!owned_ || !IsWindow(window) || !kActivateMessage ||
            !SetPropW(window, identity_.c_str(), reinterpret_cast<HANDLE>(1))) {
            return false;
        }
        window_ = window;
        return true;
    }
    bool NotifyExisting(DWORD timeout_ms, std::wstring* error) const {
        const auto started = GetTickCount64();
        do {
            Search search{identity_.c_str(), nullptr};
            EnumWindows(FindOwner, reinterpret_cast<LPARAM>(&search));
            if (search.window) {
                DWORD pid = 0U;
                GetWindowThreadProcessId(search.window, &pid);
                AllowSetForegroundWindow(pid);
                DWORD_PTR result = 0U;
                const auto elapsed = GetTickCount64() - started;
                const DWORD remaining = elapsed < timeout_ms ? timeout_ms - static_cast<DWORD>(elapsed) : 1U;
                const DWORD slice = remaining < 200U ? remaining : 200U;
                if (SendMessageTimeoutW(search.window, kActivateMessage, 0U, 0U,
                    SMTO_ABORTIFHUNG | SMTO_BLOCK, slice, &result) && result == 1U) {
                    return true;
                }
            }
            if (GetTickCount64() - started >= timeout_ms) {
                break;
            }
            Sleep(20U);
        } while (true);
        if (error) {
            *error = L"The Tool already owns this Windows session, but its window is not ready or is not responding. No second Tool was opened.";
        }
        return false;
    }
    void Release() {
        if (window_ && IsWindow(window_)) {
            RemovePropW(window_, identity_.c_str());
        }
        window_ = nullptr;
        if (mutex_) {
            if (owned_) {
                ReleaseMutex(mutex_);
            }
            CloseHandle(mutex_);
        }
        mutex_ = nullptr;
        owned_ = false;
    }
private:
    struct Search { const wchar_t* identity; HWND window; };
    static BOOL CALLBACK FindOwner(HWND window, LPARAM data) {
        auto& search = *reinterpret_cast<Search*>(data);
        if (GetPropW(window, search.identity)) {
            search.window = window;
            return FALSE;
        }
        return TRUE;
    }
    std::wstring identity_;
    HANDLE mutex_ = nullptr;
    HWND window_ = nullptr;
    bool owned_ = false;
};

inline LRESULT Activate(HWND main) {
    GUITHREADINFO info = {sizeof(info)};
    if (GetGUIThreadInfo(GetCurrentThreadId(), &info) &&
        (info.flags & (GUI_INMENUMODE | GUI_INMOVESIZE))) {
        return 1;
    }
    if (IsIconic(main)) {
        ShowWindow(main, SW_RESTORE);
    }
    HWND popup = GetLastActivePopup(main);
    HWND target = popup && IsWindowVisible(popup) ? popup : main;
    SetForegroundWindow(target);
    return 1;
}

inline void ShowStartupFailure(const std::wstring& error) {
    OutputDebugStringW(error.c_str());
    Guard notice(std::wstring(kIdentity) + L".Notice");
    if (notice.Acquire(nullptr) == StartResult::Owner) {
        MessageBoxW(nullptr, error.c_str(), L"Firmware Upgrade Tool", MB_OK | MB_ICONINFORMATION);
    }
}
}
