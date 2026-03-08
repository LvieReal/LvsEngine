#include "Lvs/Engine/Rendering/Common/PostProcessSettingsUtils.hpp"

#include <algorithm>
#include <cmath>

namespace Lvs::Engine::Rendering::Common {

std::uint32_t ComputePostProcessBlurLevels(const float blurAmount, const std::uint32_t availableLevels) {
    if (availableLevels == 0) {
        return 0;
    }
    const float clampedBlur = std::max(blurAmount, kPostProcessMinNeonBlur);
    const std::uint32_t requested = static_cast<std::uint32_t>(std::ceil(clampedBlur)) + 1U;
    return std::max(1U, std::min(availableLevels, requested));
}

} // namespace Lvs::Engine::Rendering::Common
