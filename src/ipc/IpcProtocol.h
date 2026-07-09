#pragma once
#include <windows.h>

#include <string>

namespace cwin {

constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\cwin.host";

// Taskbar layout reported by the injected CWIN.Shell.dll to the host.
struct TaskbarReport {
    RECT taskbarRect{};
    RECT reservedRect{};  // gap carved for the capsules (empty if none)
    int alignment = 0;    // 0 = center, 1 = left
    UINT dpi = 96;
    int autoHide = 0;
    int reserved = 0;     // 1 = gap physically carved in the taskbar
};

// Minimal, dependency-free line JSON for the internal layout channel.
std::string SerializeTaskbarReport(const TaskbarReport& report);
bool ParseTaskbarReport(const std::string& json, TaskbarReport& out);

} // namespace cwin
