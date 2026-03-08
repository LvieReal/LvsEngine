#pragma once

#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

constexpr std::uint32_t kPostProcessMaxBlurLevels = 4U;
constexpr float kPostProcessMinNeonBlur = 0.0F;
constexpr float kPostProcessDefaultNeonBlur = 2.0F;

[[nodiscard]] std::uint32_t ComputePostProcessBlurLevels(float blurAmount, std::uint32_t availableLevels);

} // namespace Lvs::Engine::Rendering::Common
