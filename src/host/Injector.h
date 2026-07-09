#pragma once
#include <windows.h>

#include <string>

namespace cwin {

// Injects CWIN.Shell.dll into explorer.exe via CreateRemoteThread + LoadLibraryW.
// The injected DLL measures the taskbar and reports layout back over IPC.
class Injector {
public:
    // Returns true if the DLL is loaded in explorer (either freshly injected
    // or already present). On failure the caller should fall back to companion
    // (overlay) mode, which needs no injection.
    static bool InjectIntoExplorer(const std::wstring& dllPath);

    // Absolute path to CWIN.Shell.dll next to the running host executable.
    static std::wstring DefaultShellDllPath();

private:
    static DWORD FindExplorerPid();
};

} // namespace cwin
