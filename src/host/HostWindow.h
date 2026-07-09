#pragma once
#include <windows.h>

#include <mutex>

#include "Backdrop.h"
#include "CapsuleScheduler.h"
#include "Config.h"
#include "IpcProtocol.h"
#include "Renderer.h"

namespace cwin {

// Borderless always-on-top layered window that hugs the taskbar and renders
// the capsules (companion-window mode; injection reserves the space).
class HostWindow {
public:
    bool Create(HINSTANCE instance, const Config& config);
    HWND Handle() const { return hwnd_; }
    int RunMessageLoop();

    // Thread-safe: called from the IPC worker; marshals to the UI thread.
    void PostTaskbarReport(const TaskbarReport& report);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void OnTick();
    void ApplyTaskbarReport();

    HWND hwnd_ = nullptr;
    Renderer renderer_;
    CapsuleScheduler scheduler_;
    std::mutex reportMutex_;
    TaskbarReport pendingReport_{};
    bool hasReport_ = false;
};

} // namespace cwin
