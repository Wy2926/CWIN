#pragma once
#include <string>
#include <vector>

namespace cwin {

enum class Theme { System, Light, Dark };

struct CapsuleConfig {
    std::wstring id;        // "hardware" | "weather" | "clock" | "netspeed"
    bool enabled = true;
    int order = 0;
};

struct Config {
    Theme theme = Theme::System;
    int rotationIntervalSec = 10;
    bool autoStart = false;
    std::vector<CapsuleConfig> capsules;

    static Config LoadOrDefault();
    void Save() const;
    static std::wstring ConfigPath(); // %APPDATA%\CWIN\config.json
};

} // namespace cwin
