#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

namespace Lvs::Engine::Rendering::Common {

class SceneRenderer;

struct RenderProxyPolicy {
    bool Visible{false};
    bool Transparent{false};
    bool CastsShadow{false};
};

class RenderProxy {
public:
    virtual ~RenderProxy() = default;

    virtual void SyncFromRenderer(SceneRenderer& renderer) = 0;
    virtual void Draw(CommandBuffer& commandBuffer, SceneRenderer& renderer) = 0;
    [[nodiscard]] virtual RenderProxyPolicy GetPolicy() const = 0;
    [[nodiscard]] virtual Math::Vector3 GetWorldPosition() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
