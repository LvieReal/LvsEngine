#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

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

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 3> kShadowVolumeCapModeValues{{
    {"FrontNear_BackFar", static_cast<int>(Lvs::Engine::Enums::ShadowVolumeCapMode::FrontNear_BackFar), "FrontNear_BackFar", "Cap front-facing volumes at the near plane and back-facing volumes at the far plane."},
    {"BackNear_FrontFar", static_cast<int>(Lvs::Engine::Enums::ShadowVolumeCapMode::BackNear_FrontFar), "BackNear_FrontFar", "Cap back-facing volumes at the near plane and front-facing volumes at the far plane."},
    {"None", static_cast<int>(Lvs::Engine::Enums::ShadowVolumeCapMode::None), "None", "Disable volume caps."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::ShadowVolumeCapMode> {
    static constexpr const char* Description = "Shadow volume cap strategy.";
    static constexpr const EnumValueMetadata* Values = kShadowVolumeCapModeValues.data();
    static constexpr std::size_t ValueCount = kShadowVolumeCapModeValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
