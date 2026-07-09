#pragma once
#include <windows.h>

namespace cwin {

enum class BackdropKind {
    DCompBackdrop,  // 22H2+: DirectComposition backdrop brush
    AccentBlur,     // 21H2 fallback: DWM SetWindowCompositionAttribute
    None,
};

// Probes OS capabilities and applies the best available blur-behind effect.
BackdropKind DetectBackdropKind();
bool ApplyBackdrop(HWND hwnd, BackdropKind kind);

} // namespace cwin
