#include "Lvs/Engine/Math/Projection.hpp"

#include <algorithm>
#include <cmath>

namespace Lvs::Engine::Math::Projection {

Matrix4 ReversedInfinitePerspective(const double fovYDeg, const double aspect, const double nearPlane) {
    const double f = 1.0 / std::tan(std::clamp(fovYDeg, 0.1, 179.9) * 0.5 * (3.14159265358979323846 / 180.0));

    return Matrix4({{
        {f / aspect, 0.0, 0.0, 0.0},
        {0.0, f, 0.0, 0.0},
        {0.0, 0.0, 0.0, std::max(nearPlane, 0.0001)},
        {0.0, 0.0, -1.0, 0.0}
    }});
}

} // namespace Lvs::Engine::Math::Projection
