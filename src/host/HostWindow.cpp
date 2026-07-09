#include "HostWindow.h"

namespace cwin {

namespace {
constexpr wchar_t kClassName[] = L"CWIN.HostWindow";
constexpr int kDefaultWidth = 360;
constexpr int kDefaultHeight = 48;
constexpr UINT_PTR kTickTimerId = 1;
constexpr UINT kTickIntervalMs = 1000;
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

    if (FAILED(renderer_.Initialize(hwnd_))) return false;
    ApplyBackdrop(hwnd_, DetectBackdropKind());

    scheduler_.Tick();
    renderer_.DrawCapsules(scheduler_.VisibleCapsules());
    renderer_.Commit();

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
