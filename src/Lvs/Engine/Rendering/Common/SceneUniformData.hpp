#pragma once

#include <array>

namespace Lvs::Engine::Rendering::Common {

struct CameraUniformData {
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

struct ShadowUniformData {
    std::array<std::array<float, 16>, 3> LightViewProjection{};
};

struct DrawPushConstants {
    std::array<float, 16> Model{};
    std::array<float, 4> BaseColor{};
    std::array<float, 4> Material{};
    std::array<float, 4> SurfaceData0{};
    std::array<float, 4> SurfaceData1{};
};

struct ShadowPushConstants {
    std::array<float, 16> Model{};
    std::array<float, 4> Cascade{}; // x: cascade index
};

struct SkyboxPushConstants {
    std::array<float, 16> ViewProjection{};
    std::array<float, 4> Tint{};
};

struct PostProcessPushConstants {
    std::array<float, 4> Settings{}; // x: gamma, y: dithering, z: neonEnabled, w: frameSeed
};

} // namespace Lvs::Engine::Rendering::Common
