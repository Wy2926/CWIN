#include <windows.h>

#include "Config.h"
#include "HostWindow.h"
#include "Injector.h"
#include "IpcProtocol.h"
#include "IpcServer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    cwin::Config config = cwin::Config::LoadOrDefault();

    cwin::HostWindow window;
    if (!window.Create(instance, config)) return 1;

    // Injected shell reports taskbar layout over the pipe; reposition to match.
    cwin::IpcServer ipc;
    ipc.Start(cwin::kPipeName, [&window](const std::string& request) -> std::string {
        cwin::TaskbarReport report;
        if (cwin::ParseTaskbarReport(request, report)) {
            window.PostTaskbarReport(report);
        }
        return std::string();
    });

    // Inject to get the taskbar report; on failure we stay in companion mode.
    cwin::Injector::InjectIntoExplorer(cwin::Injector::DefaultShellDllPath());

    const int rc = window.RunMessageLoop();
    ipc.Stop();
    return rc;
}
