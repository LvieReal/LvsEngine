#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class LightingTechnology {
    Default = 0
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::LightingTechnology> {
    static constexpr std::string_view Name = "LightingTechnology";
};
