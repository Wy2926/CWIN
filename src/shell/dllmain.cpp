#include <windows.h>

#include <atomic>

#include "IpcClient.h"
#include "IpcProtocol.h"
#include "TaskbarAdapter.h"

namespace {

std::atomic<bool> g_running{false};
HANDLE g_thread = nullptr;

// Measures the taskbar and pushes reports to the host. Runs entirely inside
// explorer.exe; performs no rendering (companion window in the host draws).
DWORD WINAPI ReportThread(LPVOID) {
    auto adapter = cwin::CreateTaskbarAdapter();
    cwin::TaskbarReport last{};
    bool hasLast = false;

    while (g_running) {
        cwin::TaskbarLayout layout;
        if (adapter && adapter->QueryLayout(layout)) {
            cwin::TaskbarReport report;
            report.taskbarRect = layout.taskbarRect;
            report.alignment =
                layout.alignment == cwin::TaskbarAlignment::Left ? 1 : 0;
            report.dpi = layout.dpi;
            report.autoHide = layout.autoHide ? 1 : 0;

            const bool changed =
                !hasLast ||
                memcmp(&report.taskbarRect, &last.taskbarRect, sizeof(RECT)) != 0 ||
                report.alignment != last.alignment || report.dpi != last.dpi ||
                report.autoHide != last.autoHide;
            if (changed) {
                cwin::IpcClient::Send(cwin::kPipeName,
                                      cwin::SerializeTaskbarReport(report));
                last = report;
                hasLast = true;
            }
        }
        Sleep(1000);
    }
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            g_running = true;
            g_thread = CreateThread(nullptr, 0, ReportThread, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            g_running = false;
            break;
        default:
            break;
    }
    return TRUE;
}
