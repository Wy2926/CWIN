#include <windows.h>

#include <atomic>

#include "IpcClient.h"
#include "IpcProtocol.h"
#include "Log.h"
#include "TaskbarAdapter.h"

namespace {

std::atomic<bool> g_running{false};
HANDLE g_thread = nullptr;

// Width of the capsule gap carved out of the taskbar. Matches the host's
// default capsule strip width; TODO: sync from config over IPC.
constexpr int kReserveWidthPx = 360;

// Measures the taskbar, carves the capsule gap, and pushes reports to the
// host. Runs entirely inside explorer.exe; performs no rendering (the host's
// companion window draws). Reservation is re-applied every tick because
// explorer relayouts the task band on its own updates.
DWORD WINAPI ReportThread(LPVOID) {
    cwin::Log::Init(L"shell");
    CWIN_LOG("shell attached to explorer, report thread up");
    auto adapter = cwin::CreateTaskbarAdapter();
    cwin::TaskbarReport last{};
    bool hasLast = false;

    while (g_running) {
        cwin::TaskbarLayout layout;
        const bool measured = adapter && adapter->QueryLayout(layout);
        if (measured) {
            const bool carved = adapter->ReserveSpace(kReserveWidthPx, layout);

            cwin::TaskbarReport report;
            report.taskbarRect = layout.taskbarRect;
            report.reservedRect = layout.reservedRect;
            report.alignment =
                layout.alignment == cwin::TaskbarAlignment::Left ? 1 : 0;
            report.dpi = layout.dpi;
            report.autoHide = layout.autoHide ? 1 : 0;
            report.reserved = carved ? 1 : 0;

            const bool changed =
                !hasLast ||
                memcmp(&report.taskbarRect, &last.taskbarRect, sizeof(RECT)) != 0 ||
                memcmp(&report.reservedRect, &last.reservedRect, sizeof(RECT)) != 0 ||
                report.alignment != last.alignment || report.dpi != last.dpi ||
                report.autoHide != last.autoHide || report.reserved != last.reserved;
            if (changed) {
                const bool sent = cwin::IpcClient::Send(
                    cwin::kPipeName, cwin::SerializeTaskbarReport(report));
                CWIN_LOG("report tb=(%ld,%ld,%ld,%ld) carved=%d send=%d",
                         report.taskbarRect.left, report.taskbarRect.top,
                         report.taskbarRect.right, report.taskbarRect.bottom,
                         report.reserved, sent ? 1 : 0);
                last = report;
                hasLast = true;
            }
        } else {
            CWIN_LOG("QueryLayout failed (no Shell_TrayWnd?)");
        }
        // Short sleeps so shutdown/restore happens promptly.
        for (int i = 0; i < 10 && g_running; ++i) Sleep(100);
    }

    if (adapter) adapter->Restore();
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
