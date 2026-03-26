#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class LightingTechnology {
    Default = 0
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::LightingTechnology> {
    static constexpr std::string_view Name = "LightingTechnology";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 1> kLightingTechnologyValues{{
    {"Default", static_cast<int>(Lvs::Engine::Enums::LightingTechnology::Default), "Default", "Use the engine default lighting technology."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::LightingTechnology> {
    static constexpr const char* Description = "Lighting backend/technique selection.";
    static constexpr const EnumValueMetadata* Values = kLightingTechnologyValues.data();
    static constexpr std::size_t ValueCount = kLightingTechnologyValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
