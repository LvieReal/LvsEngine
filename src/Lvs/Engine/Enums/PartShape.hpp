#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

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
