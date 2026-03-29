#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class Tonemapper {
    None = 1,
    Compression = 2,
    ACES = 3,
    AGX = 4
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::Tonemapper> {
    static constexpr std::string_view Name = "Tonemapper";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 4> kTonemapperValues{{
    {"None", static_cast<int>(Lvs::Engine::Enums::Tonemapper::None), "None", "No tonemapping (gamma only)."},
    {"Compression", static_cast<int>(Lvs::Engine::Enums::Tonemapper::Compression), "Compression", "Simple HDR compression curve."},
    {"ACES", static_cast<int>(Lvs::Engine::Enums::Tonemapper::ACES), "ACES", "ACES fitted tonemapper."},
    {"AGX", static_cast<int>(Lvs::Engine::Enums::Tonemapper::AGX), "AgX", "AgX-style tonemapping curve."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::Tonemapper> {
    static constexpr const char* Description = "Post-process tonemapping operator.";
    static constexpr const EnumValueMetadata* Values = kTonemapperValues.data();
    static constexpr std::size_t ValueCount = kTonemapperValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata

