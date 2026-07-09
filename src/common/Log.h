#pragma once
#include <string>

namespace cwin {

// Lightweight append-only diagnostic log at %APPDATA%\CWIN\<name>.log.
// Enabled only when the env var CWIN_DEBUG=1 (or a debug build), so release
// runs stay silent and zero-overhead.
class Log {
public:
    // `name` selects the file (e.g. "host", "shell"). Safe to call repeatedly.
    static void Init(const wchar_t* name);

    static void Write(const char* fmt, ...);
};

} // namespace cwin

// Usage: CWIN_LOG("report x=%d y=%d", x, y);
#define CWIN_LOG(...) ::cwin::Log::Write(__VA_ARGS__)
