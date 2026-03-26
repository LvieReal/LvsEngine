#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class RenderDepthCompare {
    Always = 0,
    Equal = 1,
    NotEqual = 2,
    Less = 3,
    LessOrEqual = 4,
    Greater = 5,
    GreaterOrEqual = 6
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::RenderDepthCompare> {
    static constexpr std::string_view Name = "RenderDepthCompare";
};

