#include "IpcServer.h"

namespace cwin {

bool IpcServer::Start(const std::wstring& /*pipeName*/, MessageHandler handler) {
    // TODO: CreateNamedPipe + overlapped accept loop.
    handler_ = std::move(handler);
    running_ = true;
    return true;
}

void IpcServer::Stop() {
    running_ = false;
}

} // namespace cwin
