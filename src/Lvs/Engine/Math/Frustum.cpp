#include "Lvs/Engine/Math/Frustum.hpp"

#include <cmath>

namespace Lvs::Engine::Math {

namespace {

Plane PlaneFromCoefficients(const double a, const double b, const double c, const double d) {
    Plane p{};
    p.Normal = {a, b, c};
    p.D = d;
    return p;
}

bool IsPlaneValid(const Plane& plane) {
    return plane.NormalLengthSquared() > 1e-18;
}

void AddPlane(Frustum& frustum, const Plane& plane) {
    if (frustum.PlaneCount >= frustum.Planes.size()) {
        return;
    }
    const Plane normalized = plane.Normalized();
    if (!IsPlaneValid(normalized)) {
        return;
    }
    frustum.Planes[frustum.PlaneCount++] = normalized;
}

} // namespace

Frustum BuildFrustumFromViewProjection(const Matrix4& viewProjection, const ClipSpaceDepthRange depthRange) {
    const auto& m = viewProjection.Rows();
    const auto row = [&](const int r, const int c) -> double { return m[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]; };

    Frustum out{};

    // For x/y, both Vulkan (0..1) and OpenGL (-1..1) use -w <= x <= w and -w <= y <= w.
    // Planes are extracted to satisfy the clip inequalities with inward-facing normals.
    AddPlane(out, PlaneFromCoefficients(row(3, 0) + row(0, 0), row(3, 1) + row(0, 1), row(3, 2) + row(0, 2), row(3, 3) + row(0, 3))); // Left
    AddPlane(out, PlaneFromCoefficients(row(3, 0) - row(0, 0), row(3, 1) - row(0, 1), row(3, 2) - row(0, 2), row(3, 3) - row(0, 3))); // Right
    AddPlane(out, PlaneFromCoefficients(row(3, 0) + row(1, 0), row(3, 1) + row(1, 1), row(3, 2) + row(1, 2), row(3, 3) + row(1, 3))); // Bottom
    AddPlane(out, PlaneFromCoefficients(row(3, 0) - row(1, 0), row(3, 1) - row(1, 1), row(3, 2) - row(1, 2), row(3, 3) - row(1, 3))); // Top

    if (depthRange == ClipSpaceDepthRange::MinusOneToOne) {
        // OpenGL: -w <= z <= w
        AddPlane(out, PlaneFromCoefficients(row(3, 0) + row(2, 0), row(3, 1) + row(2, 1), row(3, 2) + row(2, 2), row(3, 3) + row(2, 3))); // Near
        AddPlane(out, PlaneFromCoefficients(row(3, 0) - row(2, 0), row(3, 1) - row(2, 1), row(3, 2) - row(2, 2), row(3, 3) - row(2, 3))); // Far
    } else {
        // Vulkan/D3D: 0 <= z <= w
        AddPlane(out, PlaneFromCoefficients(row(2, 0), row(2, 1), row(2, 2), row(2, 3)));                                         // z >= 0
        AddPlane(out, PlaneFromCoefficients(row(3, 0) - row(2, 0), row(3, 1) - row(2, 1), row(3, 2) - row(2, 2), row(3, 3) - row(2, 3))); // z <= w
    }

    return out;
}

} // namespace Lvs::Engine::Math

