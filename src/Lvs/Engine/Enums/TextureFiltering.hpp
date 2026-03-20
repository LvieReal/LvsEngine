#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class TextureFiltering {
    Linear = 0,
    Nearest = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::TextureFiltering> {
    static constexpr std::string_view Name = "TextureFiltering";
};
