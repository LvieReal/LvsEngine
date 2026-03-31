#pragma once

#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Math/Frustum.hpp"

namespace Lvs::Engine::Math {

[[nodiscard]] inline bool IntersectsFrustumAABB(const Frustum& frustum, const AABB& aabb) {
    for (std::size_t i = 0; i < frustum.PlaneCount; ++i) {
        const Plane& plane = frustum.Planes[i];
        const Vector3 n = plane.Normal;

        const Vector3 positiveVertex{
            (n.x >= 0.0) ? aabb.Max.x : aabb.Min.x,
            (n.y >= 0.0) ? aabb.Max.y : aabb.Min.y,
            (n.z >= 0.0) ? aabb.Max.z : aabb.Min.z
        };

        if (plane.SignedDistance(positiveVertex) < 0.0) {
            return false;
        }
    }
    return true;
}

} // namespace Lvs::Engine::Math

