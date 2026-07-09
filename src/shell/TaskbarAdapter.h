#pragma once
#include <windows.h>
#include <memory>

namespace cwin {

enum class TaskbarAlignment { Center, Left };

struct TaskbarLayout {
    RECT taskbarRect{};
    RECT reservedRect{};  // area reserved for CWIN capsules
    TaskbarAlignment alignment = TaskbarAlignment::Center;
    UINT dpi = 96;
    bool autoHide = false;
};

// Abstraction over the two Win11 taskbar generations. Implementations are
// selected at runtime by build number (21H2 legacy vs 22H2+ modern).
class ITaskbarAdapter {
public:
    virtual ~ITaskbarAdapter() = default;

    // Measures the taskbar. On success fills taskbarRect/dpi/alignment/autoHide.
    virtual bool QueryLayout(TaskbarLayout& out) = 0;

    // Carves a widthPx gap just left of the system tray for the capsules and
    // fills `out.reservedRect` (screen coords). Returns true if the gap was
    // physically created in the taskbar (false = compute-only fallback, host
    // still overlays in companion mode). Idempotent; re-applied each tick.
    virtual bool ReserveSpace(int widthPx, TaskbarLayout& out) = 0;

    // Restores any taskbar layout changes made by ReserveSpace.
    virtual void Restore() = 0;
};

std::unique_ptr<ITaskbarAdapter> CreateTaskbarAdapter();

} // namespace cwin
