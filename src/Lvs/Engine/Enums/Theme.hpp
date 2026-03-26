#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class Theme {
    Light = 0,
    Dark = 1,
    Auto = 2
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::Theme> {
    static constexpr std::string_view Name = "Theme";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 3> kThemeValues{{
    {"Light", static_cast<int>(Lvs::Engine::Enums::Theme::Light), "Light", "Light theme."},
    {"Dark", static_cast<int>(Lvs::Engine::Enums::Theme::Dark), "Dark", "Dark theme."},
    {"Auto", static_cast<int>(Lvs::Engine::Enums::Theme::Auto), "Auto", "Match the system theme."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::Theme> {
    static constexpr const char* Description = "User interface theme.";
    static constexpr const EnumValueMetadata* Values = kThemeValues.data();
    static constexpr std::size_t ValueCount = kThemeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
