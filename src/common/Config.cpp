#include "Config.h"

#include <windows.h>
#include <shlobj.h>

namespace cwin {

std::wstring Config::ConfigPath() {
    PWSTR appdata = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        path = appdata;
        path += L"\\CWIN\\config.json";
        CoTaskMemFree(appdata);
    }
    return path;
}

Config Config::LoadOrDefault() {
    // TODO: parse JSON from ConfigPath(); using built-in defaults for now.
    Config cfg;
    cfg.capsules = {
        {L"hardware", true, 0},
        {L"weather", true, 1},
        {L"clock", true, 2},
        {L"netspeed", true, 3},
    };
    return cfg;
}

void Config::Save() const {
    // TODO: serialize to JSON at ConfigPath().
}

} // namespace cwin
