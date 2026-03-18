#include "Lvs/Engine/Utils/Raycast.hpp"

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <unordered_set>

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

PartBVH BuildPartBVH(const std::vector<std::shared_ptr<Objects::BasePart>>& parts) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Raycast::BuildPartBVH");
    }
    PartBVH out;
    out.Parts.reserve(parts.size());
    out.Bounds.reserve(parts.size());

    std::vector<Math::BVH::Primitive> primitives;
    primitives.reserve(parts.size());

    for (const auto& part : parts) {
        if (part == nullptr) {
            continue;
        }

        const Math::AABB aabb = BuildPartWorldAABB(part);
        const std::uint32_t payload = static_cast<std::uint32_t>(out.Parts.size());
        out.Parts.push_back(part);
        out.Bounds.push_back(aabb);
        primitives.push_back(Math::BVH::Primitive{
            .Bounds = aabb,
            .Centroid = aabb.Centroid(),
            .Payload = payload
        });
    }

    out.Bvh.Build(std::move(primitives));
    return out;
}

void RebuildPartBVH(PartBVH& bvh) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Raycast::RebuildPartBVH");
    }
    if (bvh.Parts.empty()) {
        bvh.Bounds.clear();
        bvh.Bvh.Build({});
        return;
    }

    bvh.Bounds.assign(bvh.Parts.size(), Math::AABB{});

    std::vector<Math::BVH::Primitive> primitives;
    primitives.reserve(bvh.Parts.size());

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(bvh.Parts.size()); ++i) {
        const auto& part = bvh.Parts[i];
        if (part == nullptr || part->GetParent() == nullptr) {
            continue;
        }

        const Math::AABB aabb = BuildPartWorldAABB(part);
        bvh.Bounds[i] = aabb;
        primitives.push_back(Math::BVH::Primitive{
            .Bounds = aabb,
            .Centroid = aabb.Centroid(),
            .Payload = i
        });
    }

    bvh.Bvh.Build(std::move(primitives));
}

std::pair<std::shared_ptr<Objects::BasePart>, double> RaycastPartBVH(const Ray& ray, const PartBVH& bvh) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Raycast::RaycastPartBVH");
    }
    if (bvh.Parts.empty() || bvh.Bvh.Empty()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    const auto hit = bvh.Bvh.RaycastClosest(ray.Origin, ray.Direction);
    if (!hit.has_value()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }
    if (hit->Payload >= bvh.Parts.size()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    return {bvh.Parts[hit->Payload], hit->Distance};
}

std::pair<std::shared_ptr<Objects::BasePart>, double> RaycastPartBVHWithFilter(
    const Ray& ray,
    const PartBVH& bvh,
    const std::vector<std::shared_ptr<Objects::BasePart>>& descendantFilterList,
    const DescendantFilterType filterType
) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Raycast::RaycastPartBVHWithFilter");
    }
    if (bvh.Parts.empty() || bvh.Bvh.Empty()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    if (descendantFilterList.empty()) {
        return RaycastPartBVH(ray, bvh);
    }

    std::unordered_set<const Objects::BasePart*> filterSet;
    filterSet.reserve(descendantFilterList.size());
    for (const auto& entry : descendantFilterList) {
        if (entry != nullptr) {
            filterSet.insert(entry.get());
        }
    }

    const auto hit = bvh.Bvh.RaycastClosestIf(
        ray.Origin,
        ray.Direction,
        [&](const std::uint32_t payload) {
            if (payload >= bvh.Parts.size()) {
                return false;
            }
            const auto& part = bvh.Parts[payload];
            const bool inFilter = part != nullptr && filterSet.contains(part.get());
            return (filterType == DescendantFilterType::Include) ? inFilter : !inFilter;
        }
    );

    if (!hit.has_value() || hit->Payload >= bvh.Parts.size()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    return {bvh.Parts[hit->Payload], hit->Distance};
}

std::pair<std::shared_ptr<Objects::BasePart>, double> RaycastParts(
    const Ray& ray,
    const std::vector<std::shared_ptr<Objects::BasePart>>& parts
) {
    const auto bvh = BuildPartBVH(parts);
    return RaycastPartBVH(ray, bvh);
}

std::pair<std::shared_ptr<Objects::BasePart>, double> RaycastPartsWithFilter(
    const Ray& ray,
    const std::vector<std::shared_ptr<Objects::BasePart>>& parts,
    const std::vector<std::shared_ptr<Objects::BasePart>>& descendantFilterList,
    DescendantFilterType filterType
) {
    const auto bvh = BuildPartBVH(parts);
    return RaycastPartBVHWithFilter(ray, bvh, descendantFilterList, filterType);
}

} // namespace Lvs::Engine::Utils
