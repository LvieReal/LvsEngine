#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

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

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 2> kSkyboxTextureLayoutValues{{
    {"Individual", static_cast<int>(Lvs::Engine::Enums::SkyboxTextureLayout::Individual), "Individual", "Six separate textures (one per face)."},
    {"Cross", static_cast<int>(Lvs::Engine::Enums::SkyboxTextureLayout::Cross), "Cross", "Single texture laid out as a cross."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::SkyboxTextureLayout> {
    static constexpr const char* Description = "How skybox textures are provided.";
    static constexpr const EnumValueMetadata* Values = kSkyboxTextureLayoutValues.data();
    static constexpr std::size_t ValueCount = kSkyboxTextureLayoutValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
