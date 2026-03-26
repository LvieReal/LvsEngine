#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class RenderCullMode {
    None = 0,
    Front = 1,
    Back = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::RenderCullMode> {
    static constexpr std::string_view Name = "RenderCullMode";
};

