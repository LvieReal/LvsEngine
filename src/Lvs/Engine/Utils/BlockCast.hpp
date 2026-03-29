#pragma once

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace Lvs::Engine::DataModel::Objects {
class BasePart;
}

namespace Lvs::Engine::Utils {

std::pair<std::shared_ptr<DataModel::Objects::BasePart>, double> BlockCastPartBVHWithFilter(
    const Math::CFrame& cframe,
    const Math::Vector3& size,
    const Math::Vector3& direction,
    double maxDistance,
    const PartBVH& bvh,
    const std::vector<std::shared_ptr<DataModel::Objects::BasePart>>& descendantFilterList = {},
    DescendantFilterType filterType = DescendantFilterType::Exclude,
    double originOffset = 0.0
);

} // namespace Lvs::Engine::Utils
