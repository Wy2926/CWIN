#include "Injector.h"

#include <tlhelp32.h>

#include <vector>

namespace cwin {

std::wstring Injector::DefaultShellDllPath() {
    wchar_t exePath[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath, len);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) path.resize(slash + 1);
    path += L"CWIN.Shell.dll";
    return path;
}

DWORD Injector::FindExplorerPid() {
    // The explorer instance that owns the taskbar owns Shell_TrayWnd.
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!tray) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(tray, &pid);
    return pid;
}

bool Injector::InjectIntoExplorer(const std::wstring& dllPath) {
    const DWORD pid = FindExplorerPid();
    if (pid == 0) return false;

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (!process) return false;

    bool ok = false;
    const SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_READWRITE);
    if (remote) {
        if (WriteProcessMemory(process, remote, dllPath.c_str(), bytes, nullptr)) {
            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
                reinterpret_cast<void*>(GetProcAddress(kernel32, "LoadLibraryW")));
            if (loadLibrary) {
                HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary,
                                                   remote, 0, nullptr);
                if (thread) {
                    WaitForSingleObject(thread, 5000);
                    DWORD exitCode = 0;
                    GetExitCodeThread(thread, &exitCode);
                    ok = exitCode != 0;  // LoadLibraryW returns the HMODULE
                    CloseHandle(thread);
                }
            }
        }
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    }

    CloseHandle(process);
    return ok;
}

} // namespace cwin
