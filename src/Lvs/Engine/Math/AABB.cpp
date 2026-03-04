#include "Lvs/Engine/Math/AABB.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Lvs::Engine::Math {

AABB::AABB(Vector3 minVector, Vector3 maxVector)
    : Min(std::move(minVector)),
      Max(std::move(maxVector)) {
}

Vector3 AABB::Centroid() const {
    return (Min + Max) * 0.5;
}

std::optional<double> IntersectRayAABB(
    const Vector3& origin,
    const Vector3& direction,
    const AABB& aabb,
    const double minT,
    const double maxT
) {
    double tMin = minT;
    double tMax = maxT;

    auto axisTest = [&](const double o, const double d, const double minV, const double maxV) -> bool {
        if (std::abs(d) < 1e-8) {
            return o >= minV && o <= maxV;
        }

        const double inv = 1.0 / d;
        double t1 = (minV - o) * inv;
        double t2 = (maxV - o) * inv;
        if (t1 > t2) {
            std::swap(t1, t2);
        }

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        return tMax >= tMin;
    };

    if (!axisTest(origin.x, direction.x, aabb.Min.x, aabb.Max.x)) {
        return std::nullopt;
    }
    if (!axisTest(origin.y, direction.y, aabb.Min.y, aabb.Max.y)) {
        return std::nullopt;
    }
    if (!axisTest(origin.z, direction.z, aabb.Min.z, aabb.Max.z)) {
        return std::nullopt;
    }

    if (tMax < minT) {
        return std::nullopt;
    }
    if (tMin >= minT) {
        return tMin;
    }
    return tMax;
}

AABB ComputeAABBFromPoints(const std::vector<Vector3>& points) {
    if (points.empty()) {
        throw std::runtime_error("Cannot compute AABB from empty point list.");
    }

    Vector3 minV = points.front();
    Vector3 maxV = points.front();

    for (const auto& point : points) {
        minV.x = std::min(minV.x, point.x);
        minV.y = std::min(minV.y, point.y);
        minV.z = std::min(minV.z, point.z);

        maxV.x = std::max(maxV.x, point.x);
        maxV.y = std::max(maxV.y, point.y);
        maxV.z = std::max(maxV.z, point.z);
    }

    return {minV, maxV};
}

AABB TransformAABB(const AABB& aabb, const Matrix4& matrix) {
    const Vector3 center = (aabb.Min + aabb.Max) * 0.5;
    const Vector3 extents = (aabb.Max - aabb.Min) * 0.5;
    const auto& m = matrix.Rows();

    const Vector3 worldCenter{
        m[0][0] * center.x + m[0][1] * center.y + m[0][2] * center.z + m[0][3],
        m[1][0] * center.x + m[1][1] * center.y + m[1][2] * center.z + m[1][3],
        m[2][0] * center.x + m[2][1] * center.y + m[2][2] * center.z + m[2][3]
    };

    const Vector3 worldExtents{
        std::abs(m[0][0]) * extents.x + std::abs(m[0][1]) * extents.y + std::abs(m[0][2]) * extents.z,
        std::abs(m[1][0]) * extents.x + std::abs(m[1][1]) * extents.y + std::abs(m[1][2]) * extents.z,
        std::abs(m[2][0]) * extents.x + std::abs(m[2][1]) * extents.y + std::abs(m[2][2]) * extents.z
    };

    return {worldCenter - worldExtents, worldCenter + worldExtents};
}

} // namespace Lvs::Engine::Math
