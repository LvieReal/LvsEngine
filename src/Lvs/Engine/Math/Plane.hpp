#pragma once

#include "Lvs/Engine/Math/Vector3.hpp"

#include <cmath>

namespace Lvs::Engine::Math {

struct Plane {
    Vector3 Normal{};
    double D{0.0};

    [[nodiscard]] double SignedDistance(const Vector3& point) const {
        return Normal.Dot(point) + D;
    }

    [[nodiscard]] double NormalLengthSquared() const {
        return Normal.MagnitudeSquared();
    }

    [[nodiscard]] Plane Normalized() const {
        const double lenSq = NormalLengthSquared();
        if (lenSq <= 1e-18) {
            return {};
        }
        const double invLen = 1.0 / std::sqrt(lenSq);
        return Plane{Normal * invLen, D * invLen};
    }
};

} // namespace Lvs::Engine::Math
