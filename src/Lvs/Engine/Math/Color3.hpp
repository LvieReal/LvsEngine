#pragma once

#include "Lvs/Engine/Core/Types.hpp"

namespace Lvs::Engine::Math {

struct Color3 {
    double r{1.0};
    double g{1.0};
    double b{1.0};

    [[nodiscard]] Core::String ToString() const;
    [[nodiscard]] bool operator==(const Color3& other) const;
};

} // namespace Lvs::Engine::Math
