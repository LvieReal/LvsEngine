#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class PartSurface {
    RightSurface = 0,
    LeftSurface = 1,
    TopSurface = 2,
    BottomSurface = 3,
    FrontSurface = 4,
    BackSurface = 5
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::PartSurface> {
    static constexpr std::string_view Name = "PartSurface";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 6> kPartSurfaceValues{{
    {"RightSurface", static_cast<int>(Lvs::Engine::Enums::PartSurface::RightSurface), "RightSurface", "Right face of the part."},
    {"LeftSurface", static_cast<int>(Lvs::Engine::Enums::PartSurface::LeftSurface), "LeftSurface", "Left face of the part."},
    {"TopSurface", static_cast<int>(Lvs::Engine::Enums::PartSurface::TopSurface), "TopSurface", "Top face of the part."},
    {"BottomSurface", static_cast<int>(Lvs::Engine::Enums::PartSurface::BottomSurface), "BottomSurface", "Bottom face of the part."},
    {"FrontSurface", static_cast<int>(Lvs::Engine::Enums::PartSurface::FrontSurface), "FrontSurface", "Front face of the part."},
    {"BackSurface", static_cast<int>(Lvs::Engine::Enums::PartSurface::BackSurface), "BackSurface", "Back face of the part."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::PartSurface> {
    static constexpr const char* Description = "Which face of a Part is referenced.";
    static constexpr const EnumValueMetadata* Values = kPartSurfaceValues.data();
    static constexpr std::size_t ValueCount = kPartSurfaceValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
