#include "HostWindow.h"

namespace cwin {

namespace {
constexpr wchar_t kClassName[] = L"CWIN.HostWindow";
constexpr int kDefaultWidth = 320;
constexpr int kDefaultHeight = 48;
}  // namespace

LRESULT CALLBACK HostWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

bool HostWindow::Create(HINSTANCE instance) {
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
        nullptr, nullptr, instance, nullptr);
    if (!hwnd_) return false;

    if (FAILED(renderer_.Initialize(hwnd_))) return false;
    ApplyBackdrop(hwnd_, DetectBackdropKind());
    renderer_.DrawPlaceholder();
    renderer_.Commit();

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
