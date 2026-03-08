#pragma once

#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/RenderProxy.hpp"

#include <memory>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

struct FrameRenderData {
    std::vector<std::shared_ptr<RenderProxy>> OpaqueDraws;
    std::vector<std::shared_ptr<RenderProxy>> TransparentDraws;
    std::vector<std::shared_ptr<RenderProxy>> ShadowCasters;
    Math::Vector3 CameraPosition{};
};

} // namespace Lvs::Engine::Rendering::Common

