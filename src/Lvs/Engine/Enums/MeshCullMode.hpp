#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class MeshCullMode {
    NoCull = 0,
    Back = 1,
    Front = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::MeshCullMode> {
    static constexpr std::string_view Name = "MeshCullMode";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 3> kMeshCullModeValues{{
    {"NoCull", static_cast<int>(Lvs::Engine::Enums::MeshCullMode::NoCull), "NoCull", "Render both sides of triangles."},
    {"Back", static_cast<int>(Lvs::Engine::Enums::MeshCullMode::Back), "Back", "Cull back-facing triangles."},
    {"Front", static_cast<int>(Lvs::Engine::Enums::MeshCullMode::Front), "Front", "Cull front-facing triangles."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::MeshCullMode> {
    static constexpr const char* Description = "Mesh triangle culling mode.";
    static constexpr const EnumValueMetadata* Values = kMeshCullModeValues.data();
    static constexpr std::size_t ValueCount = kMeshCullModeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
