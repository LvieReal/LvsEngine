#include "Lvs/Engine/Rendering/RenderingFactory.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderingFactory.hpp"

namespace Lvs::Engine::Rendering {

std::shared_ptr<RenderingFactory> CreateRenderingFactory(const RenderingApi requestedApi) {
    switch (ResolveSupportedRenderingApi(requestedApi)) {
        case RenderingApi::Vulkan:
        case RenderingApi::Auto:
        default:
            return std::make_shared<Vulkan::VulkanRenderingFactory>();
    }
}

} // namespace Lvs::Engine::Rendering
