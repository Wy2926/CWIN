#include "IpcServer.h"

#include <vector>

namespace cwin {

namespace {
constexpr DWORD kBufferSize = 8192;
}

IpcServer::~IpcServer() {
    Stop();
}

bool IpcServer::Start(const std::wstring& pipeName, MessageHandler handler) {
    if (running_) return false;
    pipeName_ = pipeName;
    handler_ = std::move(handler);
    running_ = true;
    worker_ = std::thread([this] { AcceptLoop(); });
    return true;
}

void IpcServer::Stop() {
    if (!running_.exchange(false)) return;
    // Unblock a pending ConnectNamedPipe by opening the pipe once.
    HANDLE h = CreateFileW(pipeName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (worker_.joinable()) worker_.join();
}

void IpcServer::AcceptLoop() {
    while (running_) {
        HANDLE pipe = CreateNamedPipeW(
            pipeName_.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, kBufferSize, kBufferSize, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) break;

        BOOL connected = ConnectNamedPipe(pipe, nullptr) ||
                         GetLastError() == ERROR_PIPE_CONNECTED;
        if (!running_) {
            CloseHandle(pipe);
            break;
        }
        if (connected) {
            std::vector<char> buffer(kBufferSize);
            DWORD read = 0;
            if (ReadFile(pipe, buffer.data(), kBufferSize, &read, nullptr) && read > 0) {
                std::string request(buffer.data(), read);
                std::string response = handler_ ? handler_(request) : std::string();
                if (!response.empty()) {
                    DWORD written = 0;
                    WriteFile(pipe, response.data(),
                              static_cast<DWORD>(response.size()), &written, nullptr);
                }
            }
            FlushFileBuffers(pipe);
            DisconnectNamedPipe(pipe);
        }
        CloseHandle(pipe);
    }
}

} // namespace cwin
