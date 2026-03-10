#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"

#include <array>
#include <cstdint>

namespace Lvs::Engine::Math {
class Vector3;
}

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Rendering::Common {

constexpr int kMaxShadowCascades = 3;

struct ShadowSettings {
    std::uint32_t MapResolution{4096U};
    float CascadeResolutionScale{0.7F};
    float CascadeSplitLambda{0.75F};
    int CascadeCount{3};
    float MaxDistance{220.0F};
    int TapCount{16};
    float BlurAmount{4.0F};
    float Bias{0.25F};
    float FadeWidth{0.25F};
};

struct ShadowCascadeComputation {
    std::array<Math::Matrix4, kMaxShadowCascades> Matrices{
        Math::Matrix4::Identity(),
        Math::Matrix4::Identity(),
        Math::Matrix4::Identity()
    };
    float Split0{0.0F};
    float Split1{0.0F};
    float MaxDistance{0.0F};
};

[[nodiscard]] ShadowSettings NormalizeShadowSettings(const ShadowSettings& settings);
[[nodiscard]] std::array<std::uint32_t, kMaxShadowCascades> ComputeCascadeResolutions(
    std::uint32_t resolution,
    float cascadeResolutionScale
);
[[nodiscard]] bool ComputeShadowCascades(
    const Objects::Camera& camera,
    const Math::Vector3& directionalLightDirection,
    float cameraAspect,
    const ShadowSettings& settings,
    const std::array<std::uint32_t, kMaxShadowCascades>& cascadeResolutions,
    ShadowCascadeComputation& out
);

} // namespace Lvs::Engine::Rendering::Common
