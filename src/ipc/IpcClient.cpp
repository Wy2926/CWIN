#include "IpcClient.h"

#include <windows.h>

namespace cwin {

bool IpcClient::Send(const std::wstring& pipeName, const std::string& message) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                  nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
            DWORD written = 0;
            BOOL ok = WriteFile(pipe, message.data(),
                                static_cast<DWORD>(message.size()), &written, nullptr);
            CloseHandle(pipe);
            return ok != FALSE;
        }
        if (GetLastError() != ERROR_PIPE_BUSY) return false;
        if (!WaitNamedPipeW(pipeName.c_str(), 1000)) return false;
    }
    return false;
}

} // namespace cwin
