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

HWND FindTray() { return FindWindowW(L"Shell_TrayWnd", nullptr); }

// The system-tray/clock cluster; the capsule gap sits immediately to its left.
HWND FindTrayNotify(HWND tray) {
    return tray ? FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr) : nullptr;
}

// Classic (21H2) task-band container we shrink to open the gap.
HWND FindReBar(HWND tray) {
    return tray ? FindWindowExW(tray, nullptr, L"ReBarWindow32", nullptr) : nullptr;
}

class TaskbarAdapterBase : public ITaskbarAdapter {
public:
    bool QueryLayout(TaskbarLayout& out) override {
        HWND tray = FindTray();
        if (!tray) return false;
        if (!GetWindowRect(tray, &out.taskbarRect)) return false;
        out.dpi = QueryDpi(tray);
        out.alignment = ReadAlignment();
        out.autoHide = ReadAutoHide();
        out.reservedRect = ComputeReservedRect(tray, out.taskbarRect, lastWidthPx_);
        return true;
    }

    void Restore() override {
        if (!reshaped_) return;
        HWND rebar = FindReBar(FindTray());
        if (rebar && (savedRebar_.right - savedRebar_.left) > 0) {
            SetWindowPos(rebar, nullptr, savedRebar_.left, savedRebar_.top,
                         savedRebar_.right - savedRebar_.left,
                         savedRebar_.bottom - savedRebar_.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        reshaped_ = false;
    }

protected:
    // Reserved gap = [trayLeft - widthPx, trayLeft) spanning taskbar height.
    static RECT ComputeReservedRect(HWND tray, const RECT& tb, int widthPx) {
        RECT reserved{};
        if (widthPx <= 0) return reserved;
        LONG gapRight = tb.right;
        RECT trayRect{};
        HWND trayNotify = FindTrayNotify(tray);
        if (trayNotify && GetWindowRect(trayNotify, &trayRect)) gapRight = trayRect.left;
        reserved.right = gapRight;
        reserved.left = gapRight - widthPx;
        reserved.top = tb.top;
        reserved.bottom = tb.bottom;
        return reserved;
    }

    int lastWidthPx_ = 0;
    bool reshaped_ = false;
    RECT savedRebar_{};
};

// Win11 21H2 classic taskbar: task band is a real HWND (ReBarWindow32) we can
// shrink so its right edge stops before the reserved gap.
class LegacyTaskbarAdapter final : public TaskbarAdapterBase {
public:
    bool ReserveSpace(int widthPx, TaskbarLayout& out) override {
        lastWidthPx_ = widthPx;
        HWND tray = FindTray();
        if (!tray) return false;
        RECT tb{};
        if (!GetWindowRect(tray, &tb)) return false;
        out.reservedRect = ComputeReservedRect(tray, tb, widthPx);
        if (widthPx <= 0) {
            Restore();
            return false;
        }

        HWND rebar = FindReBar(tray);
        if (!rebar) return false;
        RECT rebarRect{};
        if (!GetWindowRect(rebar, &rebarRect)) return false;

        if (!reshaped_) {
            savedRebar_ = MapToParent(tray, rebarRect);
            reshaped_ = true;
        }
        const RECT target = MapToParent(tray, rebarRect);
        const LONG gapLeftClient = out.reservedRect.left - tb.left;
        LONG newWidth = gapLeftClient - target.left;
        if (newWidth < 0) newWidth = 0;
        SetWindowPos(rebar, nullptr, target.left, target.top, newWidth,
                     target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
        return true;
    }

private:
    static RECT MapToParent(HWND parent, RECT screenRect) {
        POINT tl{screenRect.left, screenRect.top};
        POINT br{screenRect.right, screenRect.bottom};
        ScreenToClient(parent, &tl);
        ScreenToClient(parent, &br);
        return RECT{tl.x, tl.y, br.x, br.y};
    }
};

// Win11 22H2+ XAML taskbar: content lives in a composition island with no
// per-band HWNDs to resize. We compute the reserved rect (default center
// alignment usually leaves this area empty) and let the host overlay there.
// Physical carving is not attempted to avoid destabilizing the XAML shell.
class ModernTaskbarAdapter final : public TaskbarAdapterBase {
public:
    bool ReserveSpace(int widthPx, TaskbarLayout& out) override {
        lastWidthPx_ = widthPx;
        HWND tray = FindTray();
        if (!tray) return false;
        RECT tb{};
        if (!GetWindowRect(tray, &tb)) return false;
        out.reservedRect = ComputeReservedRect(tray, tb, widthPx);
        return false;  // compute-only; companion overlay
    }
};

}  // namespace

std::unique_ptr<ITaskbarAdapter> CreateTaskbarAdapter() {
    if (WindowsBuildNumber() >= 22621) {
        return std::make_unique<ModernTaskbarAdapter>();
    }
    return std::make_unique<LegacyTaskbarAdapter>();
}

} // namespace cwin
