#include <windows.h>

#include "Config.h"
#include "HostWindow.h"
#include "IpcServer.h"
#include "Providers.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    cwin::Config config = cwin::Config::LoadOrDefault();
    (void)config;

    cwin::IpcServer ipc;
    ipc.Start(L"\\\\.\\pipe\\cwin", [](const std::string&) { return std::string("{}"); });

    cwin::HostWindow window;
    if (!window.Create(instance)) return 1;

    const int rc = window.RunMessageLoop();
    ipc.Stop();
    return rc;
}
