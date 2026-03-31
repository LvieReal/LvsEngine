#pragma once

#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Plane.hpp"

#include <array>
#include <cstddef>

namespace Lvs::Engine::Math {

enum class ClipSpaceDepthRange {
    ZeroToOne,
    MinusOneToOne
};

struct Frustum {
    std::array<Plane, 6> Planes{};
    std::size_t PlaneCount{0};
};

[[nodiscard]] Frustum BuildFrustumFromViewProjection(const Matrix4& viewProjection, ClipSpaceDepthRange depthRange);

} // namespace Lvs::Engine::Math

