#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowDefaults.hpp"

namespace Lvs::Engine::Rendering::Common {

struct ShadowQualityProfile {
    bool Enabled{false};
    float BlurAmount{0.0F};
    int TapCount{kShadowDefaultTapCount};
    int CascadeCount{1};
    float MaxDistance{kShadowDefaultMaxDistance};
    std::uint32_t MapResolution{kShadowDefaultMapResolution};
    float CascadeResolutionScale{kShadowDefaultCascadeResolutionScale};
    float CascadeSplitLambda{kShadowDefaultCascadeSplitLambda};
};

struct RenderSettingsSnapshot {
    Math::Color3 AmbientColor{0.1, 0.1, 0.1};
    float AmbientStrength{1.0F};
    bool GammaCorrection{false};
    bool Dithering{true};
    bool InaccurateNeon{false};
    Enums::LightingComputationMode ShadingMode{Enums::LightingComputationMode::PerPixel};

    bool DirectionalLightEnabled{false};
    Math::Vector3 DirectionalLightDirection{0.0, -1.0, 0.0};
    Math::Color3 DirectionalLightColor{1.0, 1.0, 1.0};
    float DirectionalLightIntensity{0.0F};
    float DirectionalLightSpecularStrength{0.0F};
    float DirectionalLightShininess{32.0F};

    ShadowQualityProfile Shadow{};
};

} // namespace Lvs::Engine::Rendering::Common
