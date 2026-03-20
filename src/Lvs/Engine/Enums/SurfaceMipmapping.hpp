#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"

namespace Lvs::Engine::Enums {

enum class SurfaceMipmapping {
    Off = 0,
    On = 1
};

[[nodiscard]] constexpr bool IsSurfaceMipmappingEnabled(const SurfaceMipmapping value) {
    return value != SurfaceMipmapping::Off;
}

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::SurfaceMipmapping> {
    static constexpr std::string_view Name = "SurfaceMipmapping";
};
