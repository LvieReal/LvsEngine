#pragma once

#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Enums/EnumMetadataTraits.hpp"

namespace Lvs::Engine::Enums {

enum class RenderDepthCompare {
    Always = 0,
    Equal = 1,
    NotEqual = 2,
    Less = 3,
    LessOrEqual = 4,
    Greater = 5,
    GreaterOrEqual = 6
};

} // namespace Lvs::Engine::Enums

template <>
struct Lvs::Engine::Core::EnumTraits<Lvs::Engine::Enums::RenderDepthCompare> {
    static constexpr std::string_view Name = "RenderDepthCompare";
};

namespace Lvs::Engine::Enums::Metadata {
inline constexpr std::array<EnumValueMetadata, 7> kRenderDepthCompareValues{{
    {"Always", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::Always), "Always", "Always pass the depth test."},
    {"Equal", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::Equal), "Equal", "Pass if depths are equal."},
    {"NotEqual", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::NotEqual), "NotEqual", "Pass if depths are not equal."},
    {"Less", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::Less), "Less", "Pass if fragment depth is less."},
    {"LessOrEqual", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::LessOrEqual), "LessOrEqual", "Pass if fragment depth is less or equal."},
    {"Greater", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::Greater), "Greater", "Pass if fragment depth is greater."},
    {"GreaterOrEqual", static_cast<int>(Lvs::Engine::Enums::RenderDepthCompare::GreaterOrEqual), "GreaterOrEqual", "Pass if fragment depth is greater or equal."},
}};

template <>
struct EnumInfoTraits<Lvs::Engine::Enums::RenderDepthCompare> {
    static constexpr const char* Description = "Depth test comparison function.";
    static constexpr const EnumValueMetadata* Values = kRenderDepthCompareValues.data();
    static constexpr std::size_t ValueCount = kRenderDepthCompareValues.size();
};
} // namespace Lvs::Engine::Enums::Metadata
