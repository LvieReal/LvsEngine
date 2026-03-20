#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class SkyboxTextureLayout {
    Individual = 0,
    Cross = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::SkyboxTextureLayout> {
    static constexpr std::string_view Name = "SkyboxTextureLayout";
};
