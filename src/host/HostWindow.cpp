#include "HostWindow.h"

#include "Log.h"

namespace cwin {

namespace {
constexpr wchar_t kClassName[] = L"CWIN.HostWindow";
constexpr int kDefaultWidth = 360;
constexpr int kDefaultHeight = 48;
constexpr UINT_PTR kTickTimerId = 1;
constexpr UINT kTickIntervalMs = 1000;
constexpr UINT WM_CWIN_TASKBAR_REPORT = WM_APP + 1;
// Space near the taskbar's right edge occupied by the system tray / clock.
constexpr int kTrayReservePx = 200;
constexpr int kCapsuleMarginPx = 4;
}  // namespace

LRESULT CALLBACK HostWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<HostWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_TIMER:
            if (self && wParam == kTickTimerId) self->OnTick();
            return 0;
        case WM_CWIN_TASKBAR_REPORT:
            if (self) self->ApplyTaskbarReport();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kTickTimerId);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void HostWindow::OnTick() {
    if (scheduler_.Tick()) {
        renderer_.DrawCapsules(scheduler_.VisibleCapsules());
        renderer_.Commit();
    }
    // The taskbar is also topmost and relayouts frequently; re-assert our
    // position above it so the capsules are not occluded.
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void HostWindow::PostTaskbarReport(const TaskbarReport& report) {
    {
        std::lock_guard<std::mutex> lock(reportMutex_);
        pendingReport_ = report;
        hasReport_ = true;
    }
    if (hwnd_) PostMessageW(hwnd_, WM_CWIN_TASKBAR_REPORT, 0, 0);
}

void HostWindow::ApplyTaskbarReport() {
    TaskbarReport report;
    {
        std::lock_guard<std::mutex> lock(reportMutex_);
        if (!hasReport_) return;
        report = pendingReport_;
    }
    const RECT& tb = report.taskbarRect;
    const int tbHeight = tb.bottom - tb.top;
    if (tbHeight <= 0) return;

    const int height = tbHeight - 2 * kCapsuleMarginPx;
    const int y = tb.top + kCapsuleMarginPx;

    // Prefer the exact gap the shell carved/computed; otherwise fall back to a
    // fixed inset before the tray (companion overlay).
    int x;
    int width;
    const RECT& rr = report.reservedRect;
    if (rr.right > rr.left) {
        x = rr.left + kCapsuleMarginPx;
        width = (rr.right - rr.left) - 2 * kCapsuleMarginPx;
    } else {
        width = kDefaultWidth;
        x = (tb.right - kTrayReservePx) - width;
    }
    if (width <= 0) return;

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    const auto capsules = scheduler_.VisibleCapsules();
    const HRESULT hr = renderer_.DrawCapsules(capsules);
    renderer_.Commit();
    CWIN_LOG("apply report: pos=(%d,%d) %dx%d draw hr=0x%08lX capsules=%zu", x, y,
             width, height, hr, capsules.size());
}

bool HostWindow::Create(HINSTANCE instance, const Config& config) {
    scheduler_.InitFromConfig(config);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc)) return false;

    // Position bottom-right above the taskbar area for now; the taskbar
    // adapter will supply the real reserved rect once injection lands.
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.right - kDefaultWidth - 16;
    const int y = work.bottom - kDefaultHeight - 8;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
        kClassName, L"CWIN", WS_POPUP,
        x, y, kDefaultWidth, kDefaultHeight,
        nullptr, nullptr, instance, this);
    if (!hwnd_) return false;

    const HRESULT initHr = renderer_.Initialize(hwnd_);
    CWIN_LOG("renderer init hr=0x%08lX; window @ (%d,%d) %dx%d", initHr, x, y,
             kDefaultWidth, kDefaultHeight);
    if (FAILED(initHr)) return false;
    // NOTE: SetWindowCompositionAttribute (WCA) acrylic is incompatible with a
    // WS_EX_NOREDIRECTIONBITMAP DirectComposition window and suppresses the
    // composited content (the window renders blank). The frosted-glass look is
    // instead drawn inside the DComp visual tree by the renderer (translucent
    // card fill). A real DComp backdrop brush is future work.
    CWIN_LOG("backdrop: WCA disabled (drawn in DComp), detected kind=%d",
             static_cast<int>(DetectBackdropKind()));

    scheduler_.Tick();
    const auto capsules = scheduler_.VisibleCapsules();
    const HRESULT drawHr = renderer_.DrawCapsules(capsules);
    renderer_.Commit();
    CWIN_LOG("initial draw: %zu capsules, hr=0x%08lX", capsules.size(), drawHr);

    SetTimer(hwnd_, kTickTimerId, kTickIntervalMs, nullptr);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

int HostWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

} // namespace cwin
