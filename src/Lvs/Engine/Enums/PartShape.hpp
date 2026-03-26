#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class PartShape {
    Cube = 0,
    Sphere = 1,
    Cylinder = 2,
    Cone = 3
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::PartShape> {
    static constexpr std::string_view Name = "PartShape";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 4> kPartShapeValues{{
    {"Cube", static_cast<int>(Lvs::Engine::Enums::PartShape::Cube), "Cube", "Box-shaped part."},
    {"Sphere", static_cast<int>(Lvs::Engine::Enums::PartShape::Sphere), "Sphere", "Spherical part."},
    {"Cylinder", static_cast<int>(Lvs::Engine::Enums::PartShape::Cylinder), "Cylinder", "Cylindrical part."},
    {"Cone", static_cast<int>(Lvs::Engine::Enums::PartShape::Cone), "Cone", "Conical part."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::PartShape> {
    static constexpr const char* Description = "Base shape of a Part.";
    static constexpr const EnumValueMetadata* Values = kPartShapeValues.data();
    static constexpr std::size_t ValueCount = kPartShapeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
