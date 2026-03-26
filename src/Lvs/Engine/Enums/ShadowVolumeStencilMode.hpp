#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

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

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 2> kShadowVolumeStencilModeValues{{
    {"ZFail", static_cast<int>(Lvs::Engine::Enums::ShadowVolumeStencilMode::ZFail), "ZFail", "Z-fail (Carmack's reverse). More robust when the camera can be inside the volume."},
    {"ZPass", static_cast<int>(Lvs::Engine::Enums::ShadowVolumeStencilMode::ZPass), "ZPass", "Z-pass. Simpler, but can fail when the camera is inside the volume."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::ShadowVolumeStencilMode> {
    static constexpr const char* Description = "Shadow volume stencil algorithm.";
    static constexpr const EnumValueMetadata* Values = kShadowVolumeStencilModeValues.data();
    static constexpr std::size_t ValueCount = kShadowVolumeStencilModeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
