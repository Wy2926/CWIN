#pragma once
#include <string>
#include <vector>

namespace cwin {

enum class CapsuleTemplate {
    Text,          // primary text only
    IconText,      // small label + primary text
    Sparkline,     // series polyline + primary text
    ProgressRing,  // ring (0..1) + primary text
};

struct CapsuleRenderData {
    std::wstring id;
    CapsuleTemplate templateKind = CapsuleTemplate::Text;
    std::wstring primaryText;
    std::wstring secondaryText;
    std::vector<float> series;
    float progress = -1.0f;  // 0..1, -1 = N/A
};

} // namespace cwin
