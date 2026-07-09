#pragma once
#include <memory>
#include <string>
#include <vector>

namespace cwin {

// A capsule = data provider + render template + interaction.
// IDataProvider is the provider third of that model and the seed of the
// future out-of-process plugin SDK surface.
struct ProviderValue {
    std::wstring primaryText;    // e.g. "37%" / "21°C"
    std::wstring secondaryText;  // e.g. "CPU" / "Sunny"
    std::vector<float> series;   // sparkline data, empty if N/A
    float progress = -1.0f;      // 0..1 for progress ring, -1 if N/A
};

class IDataProvider {
public:
    virtual ~IDataProvider() = default;
    virtual std::wstring Id() const = 0;
    virtual unsigned RefreshIntervalMs() const = 0;
    virtual bool Refresh(ProviderValue& out) = 0;
};

std::unique_ptr<IDataProvider> CreateCpuProvider();       // PDH
std::unique_ptr<IDataProvider> CreateClockProvider();     // Win32 time
std::unique_ptr<IDataProvider> CreateNetSpeedProvider();  // IP Helper
std::unique_ptr<IDataProvider> CreateWeatherProvider();   // Open-Meteo via WinHTTP

} // namespace cwin
