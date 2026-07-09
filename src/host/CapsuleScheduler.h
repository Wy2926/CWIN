#pragma once
#include <windows.h>

#include <memory>
#include <vector>

#include "Capsule.h"
#include "Config.h"
#include "Providers.h"

namespace cwin {

// Owns providers, refreshes them on tick, and exposes the capsules currently
// visible according to the rotation policy.
class CapsuleScheduler {
public:
    void InitFromConfig(const Config& config);

    // Refreshes providers whose interval elapsed; returns true if anything changed.
    bool Tick();

    // Capsules to display right now (rotation window over all enabled capsules).
    std::vector<CapsuleRenderData> VisibleCapsules() const;

private:
    struct Entry {
        std::unique_ptr<IDataProvider> provider;
        CapsuleTemplate templateKind = CapsuleTemplate::Text;
        CapsuleRenderData data;
        ULONGLONG lastRefreshTick = 0;
    };

    std::vector<Entry> entries_;
    size_t rotationOffset_ = 0;
    unsigned rotationIntervalMs_ = 10000;
    ULONGLONG lastRotationTick_ = 0;
    // Dynamic-island style: one capsule at a time, rotation cycles through all.
    static constexpr size_t kVisibleCount = 1;
};

} // namespace cwin
