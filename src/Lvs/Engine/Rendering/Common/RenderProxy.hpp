#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"

namespace Lvs::Engine::Rendering::Common {

class SceneRenderer;

class RenderProxy {
public:
    virtual ~RenderProxy() = default;

    virtual void SyncFromRenderer(SceneRenderer& renderer) = 0;
    virtual void Draw(CommandBuffer& commandBuffer, SceneRenderer& renderer) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
