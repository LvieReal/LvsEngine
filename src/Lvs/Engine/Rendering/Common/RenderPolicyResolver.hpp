#pragma once

#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/FrameRenderData.hpp"

#include <memory>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

class RenderPolicyResolver {
public:
    FrameRenderData Resolve(
        const std::vector<std::shared_ptr<RenderProxy>>& proxies,
        const Math::Vector3& cameraPosition
    ) const;
};

} // namespace Lvs::Engine::Rendering::Common

