#pragma once

#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Math/BVH.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace Lvs::Engine::Math {
struct Vector3;
}

namespace Lvs::Engine::DataModel::Objects {
class BasePart;
class Camera;
}

namespace Lvs::Engine::Utils {

struct Ray {
    Math::Vector3 Origin;
    Math::Vector3 Direction;
};

enum class DescendantFilterType {
    Exclude,
    Include
};

struct PartBVH {
    std::vector<std::shared_ptr<DataModel::Objects::BasePart>> Parts;
    std::vector<Math::AABB> Bounds;
    Math::BVH Bvh;
};

Ray ScreenPointToRay(double x, double y, int width, int height, const std::shared_ptr<DataModel::Objects::Camera>& camera);
Math::AABB BuildPartWorldAABB(const std::shared_ptr<DataModel::Objects::BasePart>& part);
std::optional<double> RaycastPartAABB(const Ray& ray, const std::shared_ptr<DataModel::Objects::BasePart>& part);

[[nodiscard]] PartBVH BuildPartBVH(const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& parts);
void RebuildPartBVH(PartBVH& bvh);
std::pair<std::shared_ptr<DataModel::Objects::BasePart>, double> RaycastPartBVH(const Ray& ray, const PartBVH& bvh);
std::pair<std::shared_ptr<DataModel::Objects::BasePart>, double> RaycastPartBVHWithFilter(
    const Ray& ray,
    const PartBVH& bvh,
    const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& descendantFilterList,
    DescendantFilterType filterType
);

std::pair<std::shared_ptr<DataModel::Objects::BasePart>, double> RaycastParts(
    const Ray& ray,
    const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& parts
);
std::pair<std::shared_ptr<DataModel::Objects::BasePart>, double> RaycastPartsWithFilter(
    const Ray& ray,
    const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& parts,
    const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& descendantFilterList,
    DescendantFilterType filterType
);

} // namespace Lvs::Engine::Utils
