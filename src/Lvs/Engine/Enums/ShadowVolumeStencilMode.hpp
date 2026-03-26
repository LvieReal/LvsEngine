#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class ShadowVolumeStencilMode {
    ZFail = 0,
    ZPass = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::ShadowVolumeStencilMode> {
    static constexpr std::string_view Name = "ShadowVolumeStencilMode";
};

