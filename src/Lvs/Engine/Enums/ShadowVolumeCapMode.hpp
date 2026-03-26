#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

// How the volume is capped (near/far) based on light-facing classification.
enum class ShadowVolumeCapMode {
    FrontNear_BackFar = 0,
    BackNear_FrontFar = 1,
    None = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::ShadowVolumeCapMode> {
    static constexpr std::string_view Name = "ShadowVolumeCapMode";
};

