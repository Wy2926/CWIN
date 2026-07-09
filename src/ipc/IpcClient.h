#pragma once
#include <string>

namespace cwin {

// Fire-and-forget client for the named-pipe server. Used by the injected
// CWIN.Shell.dll to push taskbar reports to the host.
class IpcClient {
public:
    // Sends one message; returns true on successful write.
    static bool Send(const std::wstring& pipeName, const std::string& message);
};

} // namespace cwin
