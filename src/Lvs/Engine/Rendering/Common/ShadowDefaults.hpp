#pragma once

#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

constexpr int kMaxShadowCascades = 3;
constexpr int kShadowDefaultTapCount = 16;
constexpr float kShadowDefaultMaxDistance = 220.0F;
constexpr std::uint32_t kShadowDefaultMapResolution = 4096U;
constexpr float kShadowDefaultCascadeResolutionScale = 0.7F;
constexpr float kShadowDefaultCascadeSplitLambda = 0.75F;
constexpr float kShadowDefaultBias = 0.25F;
constexpr float kShadowDefaultFadeWidth = 0.25F;

} // namespace Lvs::Engine::Rendering::Common
