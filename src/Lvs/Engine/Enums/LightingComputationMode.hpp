#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class LightingComputationMode {
    PerPixel = 0,
    PerVertex = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::LightingComputationMode> {
    static constexpr std::string_view Name = "LightingComputationMode";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 2> kLightingComputationModeValues{{
    {"Per-Pixel", static_cast<int>(Lvs::Engine::Enums::LightingComputationMode::PerPixel), "Per-Pixel", "Compute lighting per pixel (higher quality, slower)."},
    {"Per-Vertex", static_cast<int>(Lvs::Engine::Enums::LightingComputationMode::PerVertex), "Per-Vertex", "Compute lighting per vertex (faster, lower quality)."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::LightingComputationMode> {
    static constexpr const char* Description = "How lighting is evaluated.";
    static constexpr const EnumValueMetadata* Values = kLightingComputationModeValues.data();
    static constexpr std::size_t ValueCount = kLightingComputationModeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
