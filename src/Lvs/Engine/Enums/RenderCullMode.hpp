#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class RenderCullMode {
    None = 0,
    Front = 1,
    Back = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::RenderCullMode> {
    static constexpr std::string_view Name = "RenderCullMode";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 3> kRenderCullModeValues{{
    {"None", static_cast<int>(Lvs::Engine::Enums::RenderCullMode::None), "None", "Disable face culling."},
    {"Front", static_cast<int>(Lvs::Engine::Enums::RenderCullMode::Front), "Front", "Cull front-facing triangles."},
    {"Back", static_cast<int>(Lvs::Engine::Enums::RenderCullMode::Back), "Back", "Cull back-facing triangles."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::RenderCullMode> {
    static constexpr const char* Description = "Rasterizer face culling mode.";
    static constexpr const EnumValueMetadata* Values = kRenderCullModeValues.data();
    static constexpr std::size_t ValueCount = kRenderCullModeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
