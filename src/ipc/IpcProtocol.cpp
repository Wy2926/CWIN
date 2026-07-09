#include "IpcProtocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cwin {

std::string SerializeTaskbarReport(const TaskbarReport& report) {
    char buf[384];
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"taskbar\",\"left\":%ld,\"top\":%ld,\"right\":%ld,"
                  "\"bottom\":%ld,\"align\":%d,\"dpi\":%u,\"autohide\":%d,"
                  "\"rleft\":%ld,\"rtop\":%ld,\"rright\":%ld,\"rbottom\":%ld,"
                  "\"reserved\":%d}",
                  report.taskbarRect.left, report.taskbarRect.top,
                  report.taskbarRect.right, report.taskbarRect.bottom,
                  report.alignment, report.dpi, report.autoHide,
                  report.reservedRect.left, report.reservedRect.top,
                  report.reservedRect.right, report.reservedRect.bottom,
                  report.reserved);
    return buf;
}

namespace {
bool ReadLong(const std::string& json, const char* key, long& out) {
    const size_t pos = json.find(key);
    if (pos == std::string::npos) return false;
    out = std::strtol(json.c_str() + pos + std::strlen(key), nullptr, 10);
    return true;
}
}  // namespace

bool ParseTaskbarReport(const std::string& json, TaskbarReport& out) {
    if (json.find("\"type\":\"taskbar\"") == std::string::npos) return false;
    long v = 0;
    if (ReadLong(json, "\"left\":", v)) out.taskbarRect.left = v;
    if (ReadLong(json, "\"top\":", v)) out.taskbarRect.top = v;
    if (ReadLong(json, "\"right\":", v)) out.taskbarRect.right = v;
    if (ReadLong(json, "\"bottom\":", v)) out.taskbarRect.bottom = v;
    if (ReadLong(json, "\"align\":", v)) out.alignment = static_cast<int>(v);
    if (ReadLong(json, "\"dpi\":", v)) out.dpi = static_cast<UINT>(v);
    if (ReadLong(json, "\"autohide\":", v)) out.autoHide = static_cast<int>(v);
    if (ReadLong(json, "\"rleft\":", v)) out.reservedRect.left = v;
    if (ReadLong(json, "\"rtop\":", v)) out.reservedRect.top = v;
    if (ReadLong(json, "\"rright\":", v)) out.reservedRect.right = v;
    if (ReadLong(json, "\"rbottom\":", v)) out.reservedRect.bottom = v;
    if (ReadLong(json, "\"reserved\":", v)) out.reserved = static_cast<int>(v);
    return true;
}

} // namespace cwin
