#include "Lvs/Engine/Math/BVH.hpp"

#include "Lvs/Engine/Math/Vector3.hpp"

#include <numeric>

namespace Lvs::Engine::Math {

namespace {

double GetAxisValue(const Vector3& v, const int axis) {
    switch (axis) {
        case 0: return v.x;
        case 1: return v.y;
        case 2: return v.z;
        default: return 0.0;
    }
}

AABB UnionAABB(const AABB& a, const AABB& b) {
    return {
        {std::min(a.Min.x, b.Min.x), std::min(a.Min.y, b.Min.y), std::min(a.Min.z, b.Min.z)},
        {std::max(a.Max.x, b.Max.x), std::max(a.Max.y, b.Max.y), std::max(a.Max.z, b.Max.z)}
    };
}

AABB BoundsFromCentroids(
    const std::vector<BVH::Primitive>& primitives,
    const std::vector<std::uint32_t>& indices,
    const std::uint32_t first,
    const std::uint32_t count
) {
    const Vector3 c0 = primitives[indices[first]].Centroid;
    Vector3 minC = c0;
    Vector3 maxC = c0;

    for (std::uint32_t i = 1; i < count; ++i) {
        const Vector3 c = primitives[indices[first + i]].Centroid;
        minC.x = std::min(minC.x, c.x);
        minC.y = std::min(minC.y, c.y);
        minC.z = std::min(minC.z, c.z);
        maxC.x = std::max(maxC.x, c.x);
        maxC.y = std::max(maxC.y, c.y);
        maxC.z = std::max(maxC.z, c.z);
    }

    return {minC, maxC};
}

int LongestAxis(const AABB& aabb) {
    const Vector3 e = aabb.Max - aabb.Min;
    if (e.x >= e.y && e.x >= e.z) {
        return 0;
    }
    if (e.y >= e.z) {
        return 1;
    }
    return 2;
}

} // namespace

void BVH::Build(std::vector<Primitive> primitives, const std::size_t leafSize) {
    primitives_ = std::move(primitives);
    nodes_.clear();
    indices_.clear();

    if (primitives_.empty()) {
        return;
    }

    leafSize_ = static_cast<std::uint32_t>(std::clamp<std::size_t>(leafSize, 1, 64));

    indices_.resize(primitives_.size());
    std::iota(indices_.begin(), indices_.end(), 0U);

    nodes_.reserve(primitives_.size() * 2);
    static_cast<void>(BuildNode(0, static_cast<std::uint32_t>(primitives_.size())));
}

std::uint32_t BVH::BuildNode(const std::uint32_t first, const std::uint32_t count) {
    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back(Node{});

    AABB bounds = primitives_[indices_[first]].Bounds;
    for (std::uint32_t i = 1; i < count; ++i) {
        bounds = UnionAABB(bounds, primitives_[indices_[first + i]].Bounds);
    }

    if (count <= leafSize_) {
        nodes_[nodeIndex] = Node{
            .Bounds = bounds,
            .Left = 0,
            .Right = 0,
            .First = first,
            .Count = count,
            .IsLeaf = 1
        };
        return nodeIndex;
    }

    const AABB centroidBounds = BoundsFromCentroids(primitives_, indices_, first, count);
    const int axis = LongestAxis(centroidBounds);

    const std::uint32_t mid = first + count / 2;
    std::nth_element(
        indices_.begin() + first,
        indices_.begin() + mid,
        indices_.begin() + first + count,
        [&](const std::uint32_t a, const std::uint32_t b) {
            return GetAxisValue(primitives_[a].Centroid, axis) < GetAxisValue(primitives_[b].Centroid, axis);
        }
    );

    const std::uint32_t left = BuildNode(first, mid - first);
    const std::uint32_t right = BuildNode(mid, first + count - mid);

    nodes_[nodeIndex] = Node{
        .Bounds = bounds,
        .Left = left,
        .Right = right,
        .First = 0,
        .Count = 0,
        .IsLeaf = 0
    };

    return nodeIndex;
}

} // namespace Lvs::Engine::Math

