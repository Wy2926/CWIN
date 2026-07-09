#include "Providers.h"

#include <windows.h>

namespace cwin {

namespace {

class ClockProvider final : public IDataProvider {
public:
    std::wstring Id() const override { return L"clock"; }
    unsigned RefreshIntervalMs() const override { return 1000; }
    bool Refresh(ProviderValue& out) override {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[16];
        swprintf_s(buf, L"%02u:%02u", st.wHour, st.wMinute);
        out.primaryText = buf;
        out.secondaryText = L"";
        return true;
    }
};

class StubProvider final : public IDataProvider {
public:
    explicit StubProvider(std::wstring id) : id_(std::move(id)) {}
    std::wstring Id() const override { return id_; }
    unsigned RefreshIntervalMs() const override { return 5000; }
    bool Refresh(ProviderValue& out) override {
        out.primaryText = L"--";
        out.secondaryText = id_;
        return true;
    }

private:
    std::wstring id_;
};

}  // namespace

std::unique_ptr<IDataProvider> CreateClockProvider() {
    return std::make_unique<ClockProvider>();
}

// TODO: PDH-based CPU/memory sampling with sparkline history.
std::unique_ptr<IDataProvider> CreateCpuProvider() {
    return std::make_unique<StubProvider>(L"hardware");
}

// TODO: GetIfTable2 delta sampling.
std::unique_ptr<IDataProvider> CreateNetSpeedProvider() {
    return std::make_unique<StubProvider>(L"netspeed");
}

// TODO: Open-Meteo fetch via WinHTTP.
std::unique_ptr<IDataProvider> CreateWeatherProvider() {
    return std::make_unique<StubProvider>(L"weather");
}

} // namespace cwin
