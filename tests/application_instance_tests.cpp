#include "../src/core/application_instance.h"
#include <iostream>
#include <vector>

namespace {
using namespace firmware_upgrade::application_instance;
unsigned cases = 0U, failures = 0U;
void Check(bool ok, const char* name) {
    ++cases;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
LRESULT CALLBACK Window(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (message == kActivateMessage) return Activate(hwnd);
    return DefWindowProcW(hwnd, message, w, l);
}
int Child(const std::wstring& id, DWORD delay, bool abandon) {
    Guard guard(id);
    std::wstring error;
    const auto start = guard.Acquire(&error);
    if (start == StartResult::Existing) return guard.NotifyExisting(2000U, &error) ? 10 : 11;
    if (start != StartResult::Owner) return 20;
    const auto started_name = L"Local\\" + id + L".Started";
    HANDLE started = OpenEventW(EVENT_MODIFY_STATE, FALSE, started_name.c_str());
    if (!started) return 21;
    SetEvent(started); CloseHandle(started);
    if (abandon) { Sleep(150U); ExitProcess(0U); }
    Sleep(delay);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = Window;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"OwnedInstanceFixture";
    RegisterClassW(&wc);
    HWND main = CreateWindowW(wc.lpszClassName, L"Owned instance fixture", WS_OVERLAPPEDWINDOW,
        0, 0, 200, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!guard.Attach(main)) return 22;
    const auto stop_name = L"Local\\" + id + L".Stop";
    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, stop_name.c_str());
    const auto start_time = GetTickCount64();
    while (stop && WaitForSingleObject(stop, 0U) == WAIT_TIMEOUT && GetTickCount64() - start_time < 10000U) {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        Sleep(2U);
    }
    if (stop) CloseHandle(stop);
    DestroyWindow(main);
    return 0;
}
HANDLE Spawn(const std::wstring& exe, const std::wstring& id, DWORD delay = 0U, bool abandon = false) {
    auto command = L"\"" + exe + L"\" child " + id + L" " + std::to_wstring(delay) + (abandon ? L" abandon" : L" normal");
    STARTUPINFOW startup = {sizeof(startup)};
    startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return nullptr;
    CloseHandle(process.hThread);
    return process.hProcess;
}
DWORD Finish(HANDLE process) {
    if (!process || WaitForSingleObject(process, 12000U) != WAIT_OBJECT_0) return 99U;
    DWORD result = 99U;
    GetExitCodeProcess(process, &result); CloseHandle(process);
    return result;
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 5 && std::wstring(argv[1]) == L"child") return Child(argv[2], wcstoul(argv[3], nullptr, 10), std::wstring(argv[4]) == L"abandon");
    if (argc != 2) return 2;
    wchar_t module[32768] = {};
    GetModuleFileNameW(nullptr, module, 32768U);
    const std::wstring own = module, renamed = argv[1];
    for (unsigned scenario = 0U; scenario < 3U; ++scenario) {
        const auto id = L"SimpleIsp.OwnedTest." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(scenario);
        const auto started_name = L"Local\\" + id + L".Started", stop_name = L"Local\\" + id + L".Stop";
        HANDLE started = CreateEventW(nullptr, TRUE, FALSE, started_name.c_str());
        HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, stop_name.c_str());
        if (scenario == 0U) {
            HANDLE owner = Spawn(own, id, 300U);
            Check(WaitForSingleObject(started, 3000U) == WAIT_OBJECT_0, "owner holds guard before UI readiness");
            Guard observer(id);
            Check(observer.Acquire(nullptr) == StartResult::Existing, "another process cannot acquire the live owner");
            std::wstring error;
            const auto began = GetTickCount64();
            Check(!observer.NotifyExisting(30U, &error) && GetTickCount64() - began < 500U && !error.empty(), "readiness timeout is bounded and never creates a second UI");
            Check(Finish(Spawn(renamed, id)) == 10U, "renamed copy waits for readiness and activates the same owner");
            SetEvent(stop); Check(Finish(owner) == 0U, "normal owner exits without losing workflow");
            Guard next(id);
            Check(next.Acquire(nullptr) == StartResult::Owner, "normal exit permits relaunch even while another handle exists");
        } else if (scenario == 1U) {
            HANDLE owner = Spawn(own, id, 0U, true);
            Check(WaitForSingleObject(started, 3000U) == WAIT_OBJECT_0, "owned crash fixture acquired lifetime guard");
            Guard observer(id);
            Check(observer.Acquire(nullptr) == StartResult::Existing, "observer keeps mutex identity during owner failure");
            Check(Finish(owner) == 0U, "only synthetic owner exits without releasing guard");
            Guard recovered(id);
            Check(recovered.Acquire(nullptr) == StartResult::Owner, "abandoned mutex recovers without stale-file deletion");
        } else {
            std::vector<HANDLE> children;
            for (unsigned i = 0U; i < 8U; ++i) children.push_back(Spawn(i & 1U ? renamed : own, id));
            unsigned existing = 0U, running = 0U;
            const auto began = GetTickCount64();
            do {
                existing = running = 0U;
                for (auto child : children) {
                    DWORD code = 99U;
                    if (child) GetExitCodeProcess(child, &code);
                    if (code == 10U) ++existing;
                    if (code == STILL_ACTIVE) ++running;
                }
                if (existing == 7U && running == 1U) break;
                Sleep(10U);
            } while (GetTickCount64() - began < 4000U);
            Check(existing == 7U && running == 1U, "simultaneous startup has exactly one owner across executable paths");
            SetEvent(stop);
            for (auto child : children) {
                const DWORD code = Finish(child);
                Check(code == 0U || code == 10U, "owned race process has graceful bounded teardown");
            }
        }
        CloseHandle(started); CloseHandle(stop);
    }
    std::cout << "Application instance: " << cases << " checks, " << failures << " failures; private identities only\n";
    return failures ? 1 : 0;
}
