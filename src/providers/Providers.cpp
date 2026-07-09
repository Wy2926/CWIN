#include "Providers.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <winhttp.h>

#include <algorithm>
#include <deque>

namespace cwin {

namespace {

constexpr size_t kSparklinePoints = 30;

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

// CPU usage via PDH ("% Processor Time"), memory via GlobalMemoryStatusEx.
// Primary text shows CPU%, sparkline shows CPU history, progress shows memory load.
class HardwareProvider final : public IDataProvider {
public:
    HardwareProvider() {
        if (PdhOpenQueryW(nullptr, 0, &query_) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterW(query_, L"\\Processor(_Total)\\% Processor Time",
                                      0, &cpuCounter_) == ERROR_SUCCESS) {
                PdhCollectQueryData(query_);  // prime: first sample is a baseline
                valid_ = true;
            }
        }
    }

    ~HardwareProvider() override {
        if (query_) PdhCloseQuery(query_);
    }

    std::wstring Id() const override { return L"hardware"; }
    unsigned RefreshIntervalMs() const override { return 1000; }

    bool Refresh(ProviderValue& out) override {
        double cpu = 0.0;
        if (valid_ && PdhCollectQueryData(query_) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE value{};
            if (PdhGetFormattedCounterValue(cpuCounter_, PDH_FMT_DOUBLE, nullptr,
                                            &value) == ERROR_SUCCESS) {
                cpu = std::clamp(value.doubleValue, 0.0, 100.0);
            }
        }

        history_.push_back(static_cast<float>(cpu));
        if (history_.size() > kSparklinePoints) history_.pop_front();

        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);

        wchar_t buf[32];
        swprintf_s(buf, L"%.0f%%", cpu);
        out.primaryText = buf;
        out.secondaryText = L"CPU";
        out.series.assign(history_.begin(), history_.end());
        out.progress = static_cast<float>(mem.dwMemoryLoad) / 100.0f;
        return true;
    }

private:
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER cpuCounter_ = nullptr;
    bool valid_ = false;
    std::deque<float> history_;
};

// Total up+down throughput across physical interfaces via GetIfTable2 deltas.
class NetSpeedProvider final : public IDataProvider {
public:
    std::wstring Id() const override { return L"netspeed"; }
    unsigned RefreshIntervalMs() const override { return 1000; }

    bool Refresh(ProviderValue& out) override {
        ULONG64 inOctets = 0, outOctets = 0;
        MIB_IF_TABLE2* table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR || !table) return false;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const MIB_IF_ROW2& row = table->Table[i];
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (row.OperStatus != IfOperStatusUp) continue;
            inOctets += row.InOctets;
            outOctets += row.OutOctets;
        }
        FreeMibTable(table);

        const ULONGLONG now = GetTickCount64();
        double downBps = 0.0, upBps = 0.0;
        if (lastTick_ != 0 && now > lastTick_) {
            const double dt = (now - lastTick_) / 1000.0;
            if (inOctets >= lastIn_) downBps = (inOctets - lastIn_) / dt;
            if (outOctets >= lastOut_) upBps = (outOctets - lastOut_) / dt;
        }
        lastIn_ = inOctets;
        lastOut_ = outOctets;
        lastTick_ = now;

        out.primaryText = FormatSpeed(downBps);
        out.secondaryText = L"↓ " + FormatSpeed(downBps) + L"  ↑ " + FormatSpeed(upBps);
        return true;
    }

private:
    static std::wstring FormatSpeed(double bytesPerSec) {
        wchar_t buf[32];
        if (bytesPerSec >= 1024.0 * 1024.0) {
            swprintf_s(buf, L"%.1f MB/s", bytesPerSec / (1024.0 * 1024.0));
        } else {
            swprintf_s(buf, L"%.0f KB/s", bytesPerSec / 1024.0);
        }
        return buf;
    }

    ULONG64 lastIn_ = 0;
    ULONG64 lastOut_ = 0;
    ULONGLONG lastTick_ = 0;
};

// Open-Meteo current weather via WinHTTP. No API key required.
class WeatherProvider final : public IDataProvider {
public:
    std::wstring Id() const override { return L"weather"; }
    unsigned RefreshIntervalMs() const override { return 15 * 60 * 1000; }

    bool Refresh(ProviderValue& out) override {
        std::string body;
        if (!HttpGet(L"api.open-meteo.com",
                     L"/v1/forecast?latitude=39.9&longitude=116.4&current_weather=true",
                     body)) {
            out.primaryText = L"--";
            out.secondaryText = L"天气";
            return false;
        }

        double temperature = 0.0;
        int code = -1;
        if (!ExtractNumber(body, "\"temperature\":", temperature)) return false;
        double codeValue = 0.0;
        if (ExtractNumber(body, "\"weathercode\":", codeValue)) {
            code = static_cast<int>(codeValue);
        }

        wchar_t buf[32];
        swprintf_s(buf, L"%.0f°C", temperature);
        out.primaryText = buf;
        out.secondaryText = DescribeWeatherCode(code);
        return true;
    }

private:
    static bool HttpGet(const wchar_t* host, const wchar_t* path, std::string& body) {
        bool ok = false;
        HINTERNET session = WinHttpOpen(L"CWIN/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return false;
        HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connect) {
            HINTERNET request = WinHttpOpenRequest(
                connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (request) {
                if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(request, nullptr)) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available) {
                        std::string chunk(available, '\0');
                        DWORD read = 0;
                        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
                        chunk.resize(read);
                        body += chunk;
                    }
                    ok = !body.empty();
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connect);
        }
        WinHttpCloseHandle(session);
        return ok;
    }

    static bool ExtractNumber(const std::string& json, const char* key, double& out) {
        const size_t pos = json.find(key);
        if (pos == std::string::npos) return false;
        out = atof(json.c_str() + pos + strlen(key));
        return true;
    }

    static std::wstring DescribeWeatherCode(int code) {
        // WMO weather interpretation codes used by Open-Meteo.
        if (code == 0) return L"晴";
        if (code >= 1 && code <= 3) return L"多云";
        if (code == 45 || code == 48) return L"雾";
        if (code >= 51 && code <= 67) return L"雨";
        if (code >= 71 && code <= 77) return L"雪";
        if (code >= 80 && code <= 82) return L"阵雨";
        if (code >= 95) return L"雷暴";
        return L"天气";
    }
};

}  // namespace

std::unique_ptr<IDataProvider> CreateClockProvider() {
    return std::make_unique<ClockProvider>();
}

std::unique_ptr<IDataProvider> CreateCpuProvider() {
    return std::make_unique<HardwareProvider>();
}

std::unique_ptr<IDataProvider> CreateNetSpeedProvider() {
    return std::make_unique<NetSpeedProvider>();
}

std::unique_ptr<IDataProvider> CreateWeatherProvider() {
    return std::make_unique<WeatherProvider>();
}

} // namespace cwin
