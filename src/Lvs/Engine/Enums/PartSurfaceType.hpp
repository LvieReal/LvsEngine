#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

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
