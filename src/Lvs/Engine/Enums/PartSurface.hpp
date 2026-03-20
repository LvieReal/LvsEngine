#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

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
