#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSettingsSnapshot.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowDefaults.hpp"

#include <array>
#include <cstdint>

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Rendering::Common {

constexpr std::uint32_t kShadowMinResolution = 128U;
constexpr std::uint32_t kShadowMaxResolution = 8192U;
constexpr float kShadowMinCascadeResolutionScale = 0.25F;
constexpr float kShadowMaxCascadeResolutionScale = 1.0F;
constexpr int kShadowMinTapCount = 1;
constexpr int kShadowMaxTapCount = 64;
constexpr float kShadowMinBlurAmount = 0.0F;
constexpr float kShadowMaxBlurAmount = 12.0F;
constexpr float kShadowMinDistance = 1.0F;
constexpr float kShadowMaxDistance = 1024.0F;

struct ShadowCascadeComputation {
    std::array<Math::Matrix4, kMaxShadowCascades> Matrices{
        Math::Matrix4::Identity(),
        Math::Matrix4::Identity(),
        Math::Matrix4::Identity()
    };
    float Split0{kShadowDefaultMaxDistance};
    float Split1{kShadowDefaultMaxDistance};
    float MaxDistance{kShadowDefaultMaxDistance};
};

struct NormalizedShadowSettings {
    std::uint32_t MapResolution{kShadowDefaultMapResolution};
    float CascadeResolutionScale{kShadowDefaultCascadeResolutionScale};
    float CascadeSplitLambda{kShadowDefaultCascadeSplitLambda};
    int TapCount{kShadowDefaultTapCount};
    float BlurAmount{kShadowMinBlurAmount};
    float Bias{kShadowDefaultBias};
    float FadeWidth{kShadowDefaultFadeWidth};
    int CascadeCount{1};
    float MaxDistance{kShadowDefaultMaxDistance};
};

[[nodiscard]] NormalizedShadowSettings NormalizeShadowSettings(const ShadowQualityProfile& settings);
[[nodiscard]] std::array<std::uint32_t, kMaxShadowCascades> ComputeCascadeResolutions(
    std::uint32_t resolution,
    float cascadeResolutionScale
);
[[nodiscard]] bool ComputeShadowCascades(
    const Objects::Camera& camera,
    const Math::Vector3& directionalLightDirection,
    float cameraAspect,
    const NormalizedShadowSettings& settings,
    const std::array<std::uint32_t, kMaxShadowCascades>& cascadeResolutions,
    ShadowCascadeComputation& out
);

} // namespace Lvs::Engine::Rendering::Common
