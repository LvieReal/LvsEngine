#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

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

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 2> kShadowTypeValues{{
    {"Volumes", static_cast<int>(Lvs::Engine::Enums::ShadowType::Volumes), "Volumes", "Legacy shadow volumes (z-fail). Limited with transparency and can produce artifacts; use only for a legacy look."},
    {"Cascaded", static_cast<int>(Lvs::Engine::Enums::ShadowType::Cascaded), "Cascaded", "Cascaded shadow maps (recommended)."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::ShadowType> {
    static constexpr const char* Description = "Directional shadow rendering technique.";
    static constexpr const EnumValueMetadata* Values = kShadowTypeValues.data();
    static constexpr std::size_t ValueCount = kShadowTypeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
