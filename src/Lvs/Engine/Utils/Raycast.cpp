#include "Lvs/Engine/Utils/Raycast.hpp"

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace Lvs::Engine::Utils {

namespace {

std::array<double, 4> MulMat4Vec4(const Math::Matrix4& matrix, const std::array<double, 4>& vector) {
    const auto& m = matrix.Rows();
    return {
        m[0][0] * vector[0] + m[0][1] * vector[1] + m[0][2] * vector[2] + m[0][3] * vector[3],
        m[1][0] * vector[0] + m[1][1] * vector[1] + m[1][2] * vector[2] + m[1][3] * vector[3],
        m[2][0] * vector[0] + m[2][1] * vector[1] + m[2][2] * vector[2] + m[2][3] * vector[3],
        m[3][0] * vector[0] + m[3][1] * vector[1] + m[3][2] * vector[2] + m[3][3] * vector[3]
    };
}

} // namespace

Ray ScreenPointToRay(
    const double x,
    const double y,
    const int width,
    const int height,
    const std::shared_ptr<Objects::Camera>& camera
) {
    const int safeWidth = std::max(1, width);
    const int safeHeight = std::max(1, height);

    const double ndcX = (2.0 * x) / static_cast<double>(safeWidth) - 1.0;
    const double ndcY = 1.0 - (2.0 * y) / static_cast<double>(safeHeight);

    const auto invProjection = camera->GetInverseProjectionMatrix();
    const auto invView = camera->GetInverseViewMatrix();

    std::array<double, 4> viewPos = MulMat4Vec4(invProjection, {ndcX, ndcY, 1.0, 1.0});
    if (std::abs(viewPos[3]) > 1e-8) {
        viewPos[0] /= viewPos[3];
        viewPos[1] /= viewPos[3];
        viewPos[2] /= viewPos[3];
    }

    const std::array<double, 4> worldDir4 = MulMat4Vec4(invView, {viewPos[0], viewPos[1], viewPos[2], 0.0});
    const Math::Vector3 direction = Math::Vector3{worldDir4[0], worldDir4[1], worldDir4[2]}.Unit();
    const Math::Vector3 origin = camera->GetProperty("CFrame").value<Math::CFrame>().Position;

    return {origin, direction};
}

Math::AABB BuildPartWorldAABB(const std::shared_ptr<Objects::BasePart>& part) {
    const Math::AABB localAabb{Math::Vector3{-0.5, -0.5, -0.5}, Math::Vector3{0.5, 0.5, 0.5}};
    const auto size = part->GetProperty("Size").value<Math::Vector3>();
    const auto world = part->GetWorldCFrame().ToMatrix4() * Math::Matrix4::Scale(size);
    return Math::TransformAABB(localAabb, world);
}

std::optional<double> RaycastPartAABB(const Ray& ray, const std::shared_ptr<Objects::BasePart>& part) {
    return Math::IntersectRayAABB(ray.Origin, ray.Direction, BuildPartWorldAABB(part));
}

std::pair<std::shared_ptr<Objects::BasePart>, double> RaycastParts(
    const Ray& ray,
    const std::vector<std::shared_ptr<Objects::BasePart>>& parts
) {
    std::shared_ptr<Objects::BasePart> closestPart;
    double closestDistance = std::numeric_limits<double>::infinity();

    for (const auto& part : parts) {
        if (part == nullptr) {
            continue;
        }
        const auto distance = RaycastPartAABB(ray, part);
        if (!distance.has_value()) {
            continue;
        }
        if (distance.value() < closestDistance) {
            closestDistance = distance.value();
            closestPart = part;
        }
    }

    return {closestPart, closestDistance};
}

} // namespace Lvs::Engine::Utils
