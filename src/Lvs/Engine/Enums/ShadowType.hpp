#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class ShadowType {
    Volumes = 0,
    Cascaded = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::ShadowType> {
    static constexpr std::string_view Name = "ShadowType";
};

