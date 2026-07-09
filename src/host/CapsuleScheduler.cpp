#include "CapsuleScheduler.h"

#include <windows.h>

namespace cwin {

namespace {

std::unique_ptr<IDataProvider> CreateProvider(const std::wstring& id) {
    if (id == L"hardware") return CreateCpuProvider();
    if (id == L"weather") return CreateWeatherProvider();
    if (id == L"clock") return CreateClockProvider();
    if (id == L"netspeed") return CreateNetSpeedProvider();
    return nullptr;
}

CapsuleTemplate TemplateFor(const std::wstring& id) {
    if (id == L"hardware") return CapsuleTemplate::Sparkline;
    if (id == L"weather") return CapsuleTemplate::IconText;
    if (id == L"netspeed") return CapsuleTemplate::IconText;
    return CapsuleTemplate::Text;
}

}  // namespace

void CapsuleScheduler::InitFromConfig(const Config& config) {
    rotationIntervalMs_ = static_cast<unsigned>(config.rotationIntervalSec) * 1000;
    entries_.clear();
    for (const auto& capsuleConfig : config.capsules) {
        if (!capsuleConfig.enabled) continue;
        auto provider = CreateProvider(capsuleConfig.id);
        if (!provider) continue;
        Entry entry;
        entry.provider = std::move(provider);
        entry.templateKind = TemplateFor(capsuleConfig.id);
        entry.data.id = capsuleConfig.id;
        entry.data.templateKind = entry.templateKind;
        entries_.push_back(std::move(entry));
    }
}

bool CapsuleScheduler::Tick() {
    const ULONGLONG now = GetTickCount64();
    bool changed = false;

    for (auto& entry : entries_) {
        if (entry.lastRefreshTick != 0 &&
            now - entry.lastRefreshTick < entry.provider->RefreshIntervalMs()) {
            continue;
        }
        ProviderValue value;
        if (entry.provider->Refresh(value)) {
            entry.data.primaryText = value.primaryText;
            entry.data.secondaryText = value.secondaryText;
            entry.data.series = value.series;
            entry.data.progress = value.progress;
            changed = true;
        }
        entry.lastRefreshTick = now;
    }

    if (entries_.size() > kVisibleCount && rotationIntervalMs_ > 0) {
        if (lastRotationTick_ == 0) lastRotationTick_ = now;
        if (now - lastRotationTick_ >= rotationIntervalMs_) {
            rotationOffset_ = (rotationOffset_ + 1) % entries_.size();
            lastRotationTick_ = now;
            changed = true;
        }
    }

    return changed;
}

std::vector<CapsuleRenderData> CapsuleScheduler::VisibleCapsules() const {
    std::vector<CapsuleRenderData> visible;
    const size_t count = (std::min)(entries_.size(), kVisibleCount);
    for (size_t i = 0; i < count; ++i) {
        const size_t index = (rotationOffset_ + i) % entries_.size();
        visible.push_back(entries_[index].data);
    }
    return visible;
}

} // namespace cwin
