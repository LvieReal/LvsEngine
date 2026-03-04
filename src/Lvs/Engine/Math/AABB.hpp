#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <optional>
#include <vector>

namespace Lvs::Engine::Math {

class AABB {
public:
    AABB() = default;
    AABB(Vector3 minVector, Vector3 maxVector);

    [[nodiscard]] Vector3 Centroid() const;

    Vector3 Min;
    Vector3 Max;
};

std::optional<double> IntersectRayAABB(
    const Vector3& origin,
    const Vector3& direction,
    const AABB& aabb,
    double minT = 0.0,
    double maxT = 1.0e30
);

AABB ComputeAABBFromPoints(const std::vector<Vector3>& points);
AABB TransformAABB(const AABB& aabb, const Matrix4& matrix);

} // namespace Lvs::Engine::Math
