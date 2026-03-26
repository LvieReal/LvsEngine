#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class PartSurfaceType {
    Smooth = 0,
    Studs = 1,
    Inlets = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::PartSurfaceType> {
    static constexpr std::string_view Name = "PartSurfaceType";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 3> kPartSurfaceTypeValues{{
    {"Smooth", static_cast<int>(Lvs::Engine::Enums::PartSurfaceType::Smooth), "Smooth", "Smooth surface."},
    {"Studs", static_cast<int>(Lvs::Engine::Enums::PartSurfaceType::Studs), "Studs", "Studded surface."},
    {"Inlets", static_cast<int>(Lvs::Engine::Enums::PartSurfaceType::Inlets), "Inlets", "Inset surface."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::PartSurfaceType> {
    static constexpr const char* Description = "Surface style used by a Part face.";
    static constexpr const EnumValueMetadata* Values = kPartSurfaceTypeValues.data();
    static constexpr std::size_t ValueCount = kPartSurfaceTypeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
