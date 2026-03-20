#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class SpecularHighlightType {
    Phong = 1,
    BlinnPhong = 2,
    CookTorrance = 3
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::SpecularHighlightType> {
    static constexpr std::string_view Name = "SpecularHighlightType";
};

