#include "Lvs/Engine/Utils/BlockCast.hpp"

#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace Lvs::Engine::Utils {

namespace {

Math::AABB BuildWorldAABBForBlock(const Math::CFrame& cframe, const Math::Vector3& size) {
    const Math::AABB localAabb{Math::Vector3{-0.5, -0.5, -0.5}, Math::Vector3{0.5, 0.5, 0.5}};
    const Math::Matrix4 world = cframe.ToMatrix4() * Math::Matrix4::Scale(size);
    return Math::TransformAABB(localAabb, world);
}

} // namespace

std::pair<std::shared_ptr<DataModel::Objects::BasePart>, double> BlockCastPartBVHWithFilter(
    const Math::CFrame& cframe,
    const Math::Vector3& size,
    const Math::Vector3& direction,
    const double maxDistance,
    const PartBVH& bvh,
    const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& descendantFilterList,
    const DescendantFilterType filterType,
    const double originOffset
) {
    if (bvh.Parts.empty() || bvh.Bvh.Empty()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    const double dirLen = direction.Magnitude();
    if (!std::isfinite(dirLen) || dirLen <= 1e-12) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    const Math::Vector3 dir = direction * (1.0 / dirLen);
    const Math::AABB blockAabb = BuildWorldAABBForBlock(cframe, size);
    const Math::Vector3 inflation = (blockAabb.Max - blockAabb.Min) * 0.5;

    std::unordered_set<const DataModel::Objects::BasePart*> filterSet;
    filterSet.reserve(descendantFilterList.size());
    for (const auto& entry : descendantFilterList) {
        if (entry != nullptr) {
            filterSet.insert(entry.get());
        }
    }

    const Math::Vector3 origin = cframe.Position + dir * originOffset;
    const auto hit = bvh.Bvh.RaycastClosestInflatedIf(
        origin,
        dir,
        inflation,
        [&](const std::uint32_t payload) {
            if (payload >= bvh.Parts.size()) {
                return false;
            }
            if (filterSet.empty()) {
                return true;
            }
            const auto& part = bvh.Parts[payload];
            const bool inFilter = part != nullptr && filterSet.contains(part.get());
            return (filterType == DescendantFilterType::Include) ? inFilter : !inFilter;
        },
        0.0,
        std::max(0.0, maxDistance)
    );

    if (!hit.has_value() || hit->Payload >= bvh.Parts.size()) {
        return {nullptr, std::numeric_limits<double>::infinity()};
    }

    return {bvh.Parts[hit->Payload], hit->Distance};
}

} // namespace Lvs::Engine::Utils
