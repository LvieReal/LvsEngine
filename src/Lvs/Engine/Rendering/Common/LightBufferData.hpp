#pragma once

#include "Lvs/Engine/Rendering/Common/ShadowCascadeUtils.hpp"

#include <array>
#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

constexpr std::uint32_t kMaxLights = 64U;
constexpr std::uint32_t kMaxDirectionalLights = 64U;
constexpr std::uint32_t kMaxDirectionalShadowMaps = 2U; // Limit expensive directional shadow maps to 2.
constexpr std::uint32_t kDirectionalShadowMapCascadeCount = static_cast<std::uint32_t>(kMaxShadowCascades);
constexpr std::uint32_t kMaxDirectionalShadowMapTextures = kMaxDirectionalShadowMaps * kDirectionalShadowMapCascadeCount;

enum class GpuLightType : std::uint32_t {
    Directional = 0U,
    Point = 1U
};

enum GpuLightFlags : std::uint32_t {
    GpuLightFlagEnabled = 1U << 0U
};

// Base light struct (std430-friendly, 16-byte aligned).
struct alignas(16) GpuLight {
    std::uint32_t Type{0U};
    std::uint32_t Flags{0U};
    std::uint32_t DataIndex{0U};   // Index into the typed array for this light type.
    std::uint32_t ShadowIndex{0U}; // 0..kMaxDirectionalShadowMaps-1, or 0xFFFFFFFF for no shadows.

    std::array<float, 4> ColorIntensity{}; // rgb + intensity
    std::array<float, 4> Specular{};       // x: strength, y: shininess, z: fresnelAmount, w: highlightType
};

// Directional light extension struct.
struct alignas(16) GpuDirectionalLight {
    std::array<float, 4> Direction{}; // xyz direction, w unused

    // Shadow data for this directional light (valid when ShadowState.x > 0.5 and base.ShadowIndex != 0xFFFFFFFF).
    std::array<float, 4> ShadowCascadeSplits{}; // split0, split1, maxDistance, cascadeCount
    std::array<float, 4> ShadowParams{};        // depthBiasTexels, blurRadiusTexels, tapCount, fadeWidth
    std::array<float, 4> ShadowBiasParams{};    // normalOffsetTexels, unused, unused, unused
    std::array<float, 4> ShadowState{};         // x enabled, y jitterScaleX, z jitterScaleY, w unused

    std::array<std::array<float, 16>, kMaxShadowCascades> ShadowMatrices{};
    std::array<std::array<float, 16>, kMaxShadowCascades> ShadowInvMatrices{};
};

struct alignas(16) GpuLightBuffer {
    std::array<std::uint32_t, 4> Counts{}; // x: lightCount, y: directionalCount, z: shadowedDirectionalCount, w: reserved
    std::array<GpuLight, kMaxLights> Lights{};
    std::array<GpuDirectionalLight, kMaxDirectionalLights> DirectionalLights{};
};

} // namespace Lvs::Engine::Rendering::Common
