#pragma once

#include <string_view>

namespace Lvs::Engine::Core {

template <class E>
struct EnumTraits {
    static constexpr std::string_view Name{};
};

template <class E>
[[nodiscard]] constexpr bool HasEnumName() {
    return !EnumTraits<E>::Name.empty();
}

} // namespace Lvs::Engine::Core

