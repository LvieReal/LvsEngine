#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class TextureFiltering {
    Linear = 0,
    Nearest = 1
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::TextureFiltering> {
    static constexpr std::string_view Name = "TextureFiltering";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 2> kTextureFilteringValues{{
    {"Linear", static_cast<int>(Lvs::Engine::Enums::TextureFiltering::Linear), "Linear", "Linear filtering (smooth sampling)."},
    {"Nearest", static_cast<int>(Lvs::Engine::Enums::TextureFiltering::Nearest), "Nearest", "Nearest-neighbor filtering (pixelated sampling)."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::TextureFiltering> {
    static constexpr const char* Description = "Texture sampling filter mode.";
    static constexpr const EnumValueMetadata* Values = kTextureFilteringValues.data();
    static constexpr std::size_t ValueCount = kTextureFilteringValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
