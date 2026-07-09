#pragma once
#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace cwin {

// Message-mode named-pipe server. Serves the injected shell (taskbar reports),
// the WebView2 settings page and, later, out-of-process SDK plugins.
class IpcServer {
public:
    // Handler receives a request message and returns a response message.
    using MessageHandler = std::function<std::string(const std::string& request)>;

    ~IpcServer();

    bool Start(const std::wstring& pipeName, MessageHandler handler);
    void Stop();

private:
    void AcceptLoop();

    std::wstring pipeName_;
    MessageHandler handler_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace cwin
