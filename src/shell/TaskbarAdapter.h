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
    virtual bool QueryLayout(TaskbarLayout& out) = 0;
    virtual bool ReserveSpace(int widthPx) = 0;
};

std::unique_ptr<ITaskbarAdapter> CreateTaskbarAdapter();

} // namespace cwin
