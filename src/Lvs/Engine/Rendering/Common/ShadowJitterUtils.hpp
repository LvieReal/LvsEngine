#pragma once

#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

// Shadow PCF jitter texture dimensions (ported from archived renderer).
constexpr std::uint32_t kShadowDefaultJitterSizeXY = 16U;
constexpr std::uint32_t kShadowDefaultJitterDepth = 32U;
constexpr std::uint32_t kShadowMinJitterSizeXY = 2U;
constexpr std::uint32_t kShadowMinJitterDepth = 2U;

// Generates RGBA8 data for a 3D jitter texture:
// Each texel stores two 2D offsets packed into RGBA (a.xy, b.xy) in [-1, 1] encoded to [0, 255].
[[nodiscard]] std::vector<std::uint8_t> GenerateShadowJitterTextureData(std::uint32_t sizeXY, std::uint32_t depth);

} // namespace Lvs::Engine::Rendering::Common

