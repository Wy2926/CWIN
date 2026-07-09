#include "Log.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace cwin {

namespace {
std::mutex g_mutex;
std::wstring g_path;
bool g_enabled = false;

std::wstring LogDir() {
    PWSTR appdata = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        dir = appdata;
        dir += L"\\CWIN";
        CoTaskMemFree(appdata);
        CreateDirectoryW(dir.c_str(), nullptr);
    }
    return dir;
}
}  // namespace

void Log::Init(const wchar_t* name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    wchar_t value[8]{};
    g_enabled = GetEnvironmentVariableW(L"CWIN_DEBUG", value, 8) > 0 && value[0] == L'1';
#ifdef _DEBUG
    g_enabled = true;
#endif
    if (!g_enabled) return;
    const std::wstring dir = LogDir();
    if (dir.empty()) return;
    g_path = dir + L"\\" + name + L".log";
}

void Log::Write(const char* fmt, ...) {
    if (!g_enabled || g_path.empty()) return;

    char message[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1152];
    std::snprintf(line, sizeof(line), "[%02u:%02u:%02u.%03u] %s\r\n", st.wHour,
                  st.wMinute, st.wSecond, st.wMilliseconds, message);

    std::lock_guard<std::mutex> lock(g_mutex);
    HANDLE file = CreateFileW(g_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
    CloseHandle(file);
}

} // namespace cwin
