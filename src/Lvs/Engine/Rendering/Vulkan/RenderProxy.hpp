#pragma once

#include <vulkan/vulkan.h>

namespace Lvs::Engine::Rendering::Vulkan {

class Renderer;

class RenderProxy {
public:
    virtual ~RenderProxy() = default;
    virtual void SyncFromInstance(Renderer& renderer) = 0;
    virtual void Draw(VkCommandBuffer commandBuffer, Renderer& renderer) = 0;
};

} // namespace Lvs::Engine::Rendering::Vulkan
