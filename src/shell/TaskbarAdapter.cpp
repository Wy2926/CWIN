#include "TaskbarAdapter.h"

#include <shellapi.h>

namespace cwin {

namespace {

DWORD WindowsBuildNumber() {
    OSVERSIONINFOEXW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOEXW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        if (fn && fn(&info) == 0) return info.dwBuildNumber;
    }
    return 0;
}

// HKCU Advanced\TaskbarAl: 0 = left aligned, 1 = center aligned (Win11 default).
TaskbarAlignment ReadAlignment() {
    HKEY key;
    TaskbarAlignment alignment = TaskbarAlignment::Center;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                      0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        DWORD value = 1, size = sizeof(value), type = 0;
        if (RegQueryValueExW(key, L"TaskbarAl", nullptr, &type,
                             reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS &&
            type == REG_DWORD) {
            alignment = value == 0 ? TaskbarAlignment::Left : TaskbarAlignment::Center;
        }
        RegCloseKey(key);
    }
    return alignment;
}

bool ReadAutoHide() {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    const UINT_PTR state = SHAppBarMessage(ABM_GETSTATE, &abd);
    return (state & ABS_AUTOHIDE) != 0;
}

UINT QueryDpi(HWND hwnd) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto fn = reinterpret_cast<GetDpiForWindowFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow")));
        if (fn) return fn(hwnd);
    }
    return 96;
}

// Shared measurement; the two generations differ only in child structure,
// which matters for space reservation (TODO), not for the outer rect.
class TaskbarAdapterBase : public ITaskbarAdapter {
public:
    bool QueryLayout(TaskbarLayout& out) override {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!tray) return false;
        if (!GetWindowRect(tray, &out.taskbarRect)) return false;
        out.dpi = QueryDpi(tray);
        out.alignment = ReadAlignment();
        out.autoHide = ReadAutoHide();
        out.reservedRect = {};
        return true;
    }

    bool ReserveSpace(int /*widthPx*/) override {
        // TODO: carve capsule space out of the taskbar layout.
        return false;
    }
};

class ModernTaskbarAdapter final : public TaskbarAdapterBase {};  // 22H2+
class LegacyTaskbarAdapter final : public TaskbarAdapterBase {};  // 21H2

}  // namespace

std::unique_ptr<ITaskbarAdapter> CreateTaskbarAdapter() {
    if (WindowsBuildNumber() >= 22621) {
        return std::make_unique<ModernTaskbarAdapter>();
    }
    return std::make_unique<LegacyTaskbarAdapter>();
}

} // namespace cwin
