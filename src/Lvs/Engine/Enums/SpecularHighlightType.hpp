#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class SpecularHighlightType {
    Phong = 1,
    BlinnPhong = 2,
    CookTorrance = 3
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::SpecularHighlightType> {
    static constexpr std::string_view Name = "SpecularHighlightType";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 3> kSpecularHighlightTypeValues{{
    {"Phong", static_cast<int>(Lvs::Engine::Enums::SpecularHighlightType::Phong), "Phong", "Classic Phong specular model."},
    {"Blinn-Phong", static_cast<int>(Lvs::Engine::Enums::SpecularHighlightType::BlinnPhong), "Blinn-Phong", "Blinn-Phong specular model."},
    {"Cook-Torrance", static_cast<int>(Lvs::Engine::Enums::SpecularHighlightType::CookTorrance), "Cook-Torrance", "Microfacet (PBR-style) specular model."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::SpecularHighlightType> {
    static constexpr const char* Description = "Specular highlight BRDF.";
    static constexpr const EnumValueMetadata* Values = kSpecularHighlightTypeValues.data();
    static constexpr std::size_t ValueCount = kSpecularHighlightTypeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
