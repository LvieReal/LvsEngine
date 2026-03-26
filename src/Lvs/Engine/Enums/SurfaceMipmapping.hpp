#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class SurfaceMipmapping {
    Off = 0,
    On = 1
};

[[nodiscard]] constexpr bool IsSurfaceMipmappingEnabled(const SurfaceMipmapping value) {
    return value != SurfaceMipmapping::Off;
}

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::SurfaceMipmapping> {
    static constexpr std::string_view Name = "SurfaceMipmapping";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 2> kSurfaceMipmappingValues{{
    {"Off", static_cast<int>(Lvs::Engine::Enums::SurfaceMipmapping::Off), "Off", "Disable mipmaps."},
    {"On", static_cast<int>(Lvs::Engine::Enums::SurfaceMipmapping::On), "On", "Enable mipmaps."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::SurfaceMipmapping> {
    static constexpr const char* Description = "Controls mipmapping for surfaces/textures where applicable.";
    static constexpr const EnumValueMetadata* Values = kSurfaceMipmappingValues.data();
    static constexpr std::size_t ValueCount = kSurfaceMipmappingValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
