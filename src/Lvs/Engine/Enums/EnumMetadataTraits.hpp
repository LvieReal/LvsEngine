#pragma once

#include <array>
#include <cstddef>

namespace Lvs::Engine::Enums::Metadata {

struct EnumValueMetadata {
    const char* Name;
    int Id;
    const char* DisplayName;
    const char* Description;
};

template <typename E>
struct EnumInfoTraits {
    static constexpr const char* Description = "";
    static constexpr const EnumValueMetadata* Values = nullptr;
    static constexpr std::size_t ValueCount = 0;
};

} // namespace Lvs::Engine::Enums::Metadata

