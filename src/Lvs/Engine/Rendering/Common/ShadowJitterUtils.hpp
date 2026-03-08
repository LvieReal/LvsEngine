#pragma once

#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

constexpr std::uint32_t kShadowDefaultJitterSizeXY = 16U;
constexpr std::uint32_t kShadowDefaultJitterDepth = 32U;
constexpr std::uint32_t kShadowMinJitterSizeXY = 2U;
constexpr std::uint32_t kShadowMinJitterDepth = 2U;

[[nodiscard]] std::vector<std::uint8_t> GenerateShadowJitterTextureData(std::uint32_t sizeXY, std::uint32_t depth);

} // namespace Lvs::Engine::Rendering::Common
