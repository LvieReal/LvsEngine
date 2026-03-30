#pragma once

#include <array>
#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

struct CameraUniformData {
    std::array<float, 16> View{};
    std::array<float, 16> Projection{};
    std::array<float, 4> CameraPosition{};
    std::array<float, 4> Ambient{};
    std::array<float, 4> SkyTint{};
    std::array<float, 4> RenderSettings{};
    std::array<float, 4> LightingSettings{}; // x: perVertexShadingEnabled, yzw: shadow-volume range config
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

struct DrawInstanceData {
    std::array<float, 16> Model{};
    std::array<float, 4> BaseColor{};
    std::array<float, 4> Material{};
    std::array<float, 4> SurfaceData0{};
    std::array<float, 4> SurfaceData1{};
};

struct DrawCallPushConstants {
    std::array<std::uint32_t, 4> Data{}; // x: base instance
};

struct ShadowDrawCallPushConstants {
    std::array<std::uint32_t, 4> Data{}; // x: base instance, y: cascade index
};

struct ShadowVolumePushConstants {
    std::array<std::uint32_t, 4> Data{};         // x: base instance
    std::array<float, 4> LightDirExtrude{}; // xyz: light ray direction (world), w: extrude distance
    std::array<float, 4> Params{};         // x: bias (world units)
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
    std::array<float, 4> Settings{}; // x: tonemapper, y: dithering, z: neonEnabled, w: frameSeed
};

struct PostCompositePushConstants {
    std::array<float, 4> Settings{}; // x: tonemapper, y: dithering, z: neonEnabled, w: frameSeed
    std::array<float, 4> AoTint{};   // rgb: AO tint, a: neonAttenuation
};

struct HbaoPushConstants {
    std::array<float, 4> Params0{}; // x: radius, y: strength, z: tanBias, w: maxRadiusPixels
    std::array<float, 4> Params1{}; // x: power, y: directions, z: samples, w: reserved
    std::array<float, 4> Params2{}; // x: aoResX, y: aoResY, z: invResX, w: invResY
};

struct Image3DPushConstants {
    std::array<float, 16> Model{};
    std::array<float, 4> Color{}; // rgb: tint, a: alpha
    std::array<float, 4> Options{};      // x: negateMask, y: depthOnly, z: outlineEnabled
    std::array<float, 4> OutlineColor{}; // rgb: color, a: alpha
    std::array<float, 4> OutlineParams{}; // x: thicknessPixels, y: alphaThreshold
};

} // namespace Lvs::Engine::Rendering::Common
