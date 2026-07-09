#include <windows.h>

#include "Config.h"
#include "HostWindow.h"
#include "Injector.h"
#include "IpcProtocol.h"
#include "IpcServer.h"
#include "Log.h"

namespace {
// Match the DPI space the injected shell reports in (explorer is
// per-monitor-v2 aware, so its taskbar/reserved rects are physical pixels).
// Without this the host is DPI-virtualized and SetWindowPos misplaces the
// capsule window off-screen on scaled displays. Called before any window.
void MakePerMonitorDpiAware() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setCtx = reinterpret_cast<SetCtxFn>(
        reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
    if (setCtx) {
        setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);  // (HANDLE)-4
    }
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    MakePerMonitorDpiAware();
    cwin::Log::Init(L"host");
    CWIN_LOG("host start");

    cwin::Config config = cwin::Config::LoadOrDefault();
    CWIN_LOG("config: %zu capsules, rotation=%ds", config.capsules.size(),
             config.rotationIntervalSec);

    cwin::HostWindow window;
    if (!window.Create(instance, config)) {
        CWIN_LOG("HostWindow::Create FAILED");
        return 1;
    }

    // Injected shell reports taskbar layout over the pipe; reposition to match.
    cwin::IpcServer ipc;
    ipc.Start(cwin::kPipeName, [&window](const std::string& request) -> std::string {
        cwin::TaskbarReport report;
        if (cwin::ParseTaskbarReport(request, report)) {
            CWIN_LOG("report: tb=(%ld,%ld,%ld,%ld) rr=(%ld,%ld,%ld,%ld) reserved=%d",
                     report.taskbarRect.left, report.taskbarRect.top,
                     report.taskbarRect.right, report.taskbarRect.bottom,
                     report.reservedRect.left, report.reservedRect.top,
                     report.reservedRect.right, report.reservedRect.bottom,
                     report.reserved);
            window.PostTaskbarReport(report);
        }
        return std::string();
    });

    // Inject to get the taskbar report; on failure we stay in companion mode.
    const std::wstring dll = cwin::Injector::DefaultShellDllPath();
    const bool injected = cwin::Injector::InjectIntoExplorer(dll);
    CWIN_LOG("inject %s", injected ? "OK" : "FAILED (companion mode)");

    const int rc = window.RunMessageLoop();
    ipc.Stop();
    return rc;
}
