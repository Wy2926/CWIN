#include "TaskbarAdapter.h"

namespace cwin {

namespace {

class ModernTaskbarAdapter final : public ITaskbarAdapter {
public:
    bool QueryLayout(TaskbarLayout& out) override {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!tray) return false;
        if (!GetWindowRect(tray, &out.taskbarRect)) return false;
        out.dpi = GetDpiForWindow(tray);
        out.reservedRect = {};
        return true;
    }

    bool ReserveSpace(int /*widthPx*/) override {
        // TODO: hook taskbar layout to carve out capsule space.
        return false;
    }
};

}  // namespace

std::unique_ptr<ITaskbarAdapter> CreateTaskbarAdapter() {
    // TODO: select Legacy21H2Adapter vs ModernTaskbarAdapter by build number.
    return std::make_unique<ModernTaskbarAdapter>();
}

} // namespace cwin
