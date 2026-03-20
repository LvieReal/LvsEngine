#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class Theme {
    Light = 0,
    Dark = 1,
    Auto = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::Theme> {
    static constexpr std::string_view Name = "Theme";
};
