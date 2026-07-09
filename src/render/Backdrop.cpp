#include "Backdrop.h"

#include <dwmapi.h>

namespace cwin {

namespace {

// Undocumented but long-stable DWM API used by many taskbar tools.
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;  // AABBGGRR
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;  // 19 = WCA_ACCENT_POLICY
    PVOID pvData;
    SIZE_T cbData;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

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

}  // namespace

BackdropKind DetectBackdropKind() {
    const DWORD build = WindowsBuildNumber();
    if (build >= 22621) return BackdropKind::DCompBackdrop;  // 22H2+
    if (build >= 22000) return BackdropKind::AccentBlur;     // 21H2
    return BackdropKind::None;
}

bool ApplyBackdrop(HWND hwnd, BackdropKind kind) {
    if (kind == BackdropKind::None) return false;

    // TODO: DCompBackdrop path via composition backdrop brush; both kinds use
    // accent blur until the DComp brush pipeline is wired into Renderer.
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return false;
    auto setWca = reinterpret_cast<SetWindowCompositionAttributeFn>(
        reinterpret_cast<void*>(GetProcAddress(user32, "SetWindowCompositionAttribute")));
    if (!setWca) return false;

    ACCENT_POLICY accent{ACCENT_ENABLE_ACRYLICBLURBEHIND, 2, 0x66000000, 0};
    WINDOWCOMPOSITIONATTRIBDATA data{19 /*WCA_ACCENT_POLICY*/, &accent, sizeof(accent)};
    return setWca(hwnd, &data) != FALSE;
}

} // namespace cwin
