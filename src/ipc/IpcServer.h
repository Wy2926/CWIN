#pragma once
#include <functional>
#include <string>

namespace cwin {

// Named-pipe JSON IPC server: serves the WebView2 settings page and, later,
// out-of-process SDK plugins.
class IpcServer {
public:
    using MessageHandler = std::function<std::string(const std::string& requestJson)>;

    bool Start(const std::wstring& pipeName, MessageHandler handler);
    void Stop();

private:
    MessageHandler handler_;
    bool running_ = false;
};

} // namespace cwin
