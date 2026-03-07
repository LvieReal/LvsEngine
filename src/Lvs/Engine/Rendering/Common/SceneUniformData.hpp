#pragma once

#include <array>

namespace Lvs::Engine::Rendering::Common {

struct SceneUniformData {
    std::array<float, 16> View{};
    std::array<float, 16> Projection{};
    std::array<float, 4> CameraPosition{};
    std::array<float, 4> LightDirection{};
    std::array<float, 4> LightColorIntensity{};
    std::array<float, 4> LightSpecular{};
    std::array<float, 4> Ambient{};
    std::array<float, 4> SkyTint{};
    std::array<float, 4> RenderSettings{};
    std::array<std::array<float, 16>, 3> ShadowMatrices{};
    std::array<float, 4> ShadowCascadeSplits{};
    std::array<float, 4> ShadowParams{};
    std::array<float, 4> ShadowState{};
    std::array<float, 4> CameraForward{};
};

} // namespace Lvs::Engine::Rendering::Common
