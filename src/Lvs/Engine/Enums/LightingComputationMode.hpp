#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class LightingComputationMode {
    PerPixel = 0,
    PerVertex = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::LightingComputationMode> {
    static constexpr std::string_view Name = "LightingComputationMode";
};
