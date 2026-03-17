#pragma once

#include "Lvs/Engine/Math/AABB.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Lvs::Engine::Math {

class BVH final {
public:
    struct Primitive {
        AABB Bounds{};
        Vector3 Centroid{};
        std::uint32_t Payload{0};
    };

    struct RaycastHit {
        std::uint32_t Payload{0};
        double Distance{std::numeric_limits<double>::infinity()};
    };

    BVH() = default;

    void Build(std::vector<Primitive> primitives, std::size_t leafSize = 8);

    [[nodiscard]] bool Empty() const { return primitives_.empty() || nodes_.empty(); }
    [[nodiscard]] std::size_t Size() const { return primitives_.size(); }

    [[nodiscard]] std::optional<RaycastHit> RaycastClosest(
        const Vector3& origin,
        const Vector3& direction,
        double minT = 0.0,
        double maxT = 1.0e30
    ) const {
        return RaycastClosestIf(
            origin,
            direction,
            [](std::uint32_t) { return true; },
            minT,
            maxT
        );
    }

    template <class AcceptFn>
        requires(std::is_invocable_r_v<bool, AcceptFn, std::uint32_t>)
    [[nodiscard]] std::optional<RaycastHit> RaycastClosestIf(
        const Vector3& origin,
        const Vector3& direction,
        AcceptFn&& accept,
        double minT = 0.0,
        double maxT = 1.0e30
    ) const {
        if (Empty()) {
            return std::nullopt;
        }

        RaycastHit best{};
        best.Distance = maxT;
        bool hasBest = false;

        std::vector<std::uint32_t> stack;
        stack.reserve(nodes_.size());
        stack.push_back(0);

        while (!stack.empty()) {
            const std::uint32_t nodeIndex = stack.back();
            stack.pop_back();

            const Node& node = nodes_[nodeIndex];
            const auto nodeHit = IntersectRayAABB(origin, direction, node.Bounds, minT, best.Distance);
            if (!nodeHit.has_value()) {
                continue;
            }

            if (node.IsLeaf != 0U) {
                for (std::uint32_t i = 0; i < node.Count; ++i) {
                    const std::uint32_t primIndex = indices_[node.First + i];
                    const Primitive& prim = primitives_[primIndex];
                    if (!accept(prim.Payload)) {
                        continue;
                    }

                    const auto hit = IntersectRayAABB(origin, direction, prim.Bounds, minT, best.Distance);
                    if (!hit.has_value()) {
                        continue;
                    }

                    hasBest = true;
                    best.Payload = prim.Payload;
                    best.Distance = hit.value();
                }
                continue;
            }

            const Node& left = nodes_[node.Left];
            const Node& right = nodes_[node.Right];
            const auto leftHit = IntersectRayAABB(origin, direction, left.Bounds, minT, best.Distance);
            const auto rightHit = IntersectRayAABB(origin, direction, right.Bounds, minT, best.Distance);

            if (leftHit.has_value() && rightHit.has_value()) {
                if (leftHit.value() < rightHit.value()) {
                    stack.push_back(node.Right);
                    stack.push_back(node.Left);
                } else {
                    stack.push_back(node.Left);
                    stack.push_back(node.Right);
                }
            } else if (leftHit.has_value()) {
                stack.push_back(node.Left);
            } else if (rightHit.has_value()) {
                stack.push_back(node.Right);
            }
        }

        if (!hasBest) {
            return std::nullopt;
        }
        return best;
    }

private:
    struct Node {
        AABB Bounds{};
        std::uint32_t Left{0};
        std::uint32_t Right{0};
        std::uint32_t First{0};
        std::uint32_t Count{0};
        std::uint8_t IsLeaf{0};
    };

    std::uint32_t leafSize_{8};
    std::vector<Primitive> primitives_;
    std::vector<std::uint32_t> indices_;
    std::vector<Node> nodes_;

    std::uint32_t BuildNode(std::uint32_t first, std::uint32_t count);
};

} // namespace Lvs::Engine::Math
