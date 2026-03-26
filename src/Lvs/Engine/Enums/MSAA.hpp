#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class MSAA {
    Off = 0,
    X2 = 1,
    X4 = 2,
    X8 = 3
};

[[nodiscard]] constexpr int MsaaSampleCount(const MSAA value) {
    switch (value) {
        case MSAA::X2:
            return 2;
        case MSAA::X4:
            return 4;
        case MSAA::X8:
            return 8;
        case MSAA::Off:
        default:
            return 1;
    }
}

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::MSAA> {
    static constexpr std::string_view Name = "MSAA";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 4> kMsaaValues{{
    {"Off", static_cast<int>(Lvs::Engine::Enums::MSAA::Off), "Off", "Disable multisample anti-aliasing."},
    {"2x", static_cast<int>(Lvs::Engine::Enums::MSAA::X2), "2x", "2 samples per pixel."},
    {"4x", static_cast<int>(Lvs::Engine::Enums::MSAA::X4), "4x", "4 samples per pixel."},
    {"8x", static_cast<int>(Lvs::Engine::Enums::MSAA::X8), "8x", "8 samples per pixel."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::MSAA> {
    static constexpr const char* Description = "Multisample anti-aliasing sample count.";
    static constexpr const EnumValueMetadata* Values = kMsaaValues.data();
    static constexpr std::size_t ValueCount = kMsaaValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
