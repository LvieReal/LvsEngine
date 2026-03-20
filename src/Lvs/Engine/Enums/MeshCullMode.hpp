#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class MeshCullMode {
    NoCull = 0,
    Back = 1,
    Front = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::MeshCullMode> {
    static constexpr std::string_view Name = "MeshCullMode";
};
