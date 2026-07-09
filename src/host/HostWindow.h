#pragma once
#include <windows.h>

#include "Backdrop.h"
#include "Renderer.h"

namespace cwin {

// Borderless always-on-top layered window that hugs the taskbar and renders
// the capsules (companion-window mode; injection reserves the space).
class HostWindow {
public:
    bool Create(HINSTANCE instance);
    HWND Handle() const { return hwnd_; }
    int RunMessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND hwnd_ = nullptr;
    Renderer renderer_;
};

} // namespace cwin
