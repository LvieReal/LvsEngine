#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderingFactory.hpp"

#include "Lvs/Engine/Rendering/Vulkan/Renderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanSkyboxRenderer.hpp"

namespace Lvs::Engine::Rendering::Vulkan {

RenderingApi VulkanRenderingFactory::GetApi() const {
    return RenderingApi::Vulkan;
}

std::unique_ptr<Common::SceneRenderer> VulkanRenderingFactory::CreateSceneRenderer() {
    return std::make_unique<Renderer>(shared_from_this());
}

std::unique_ptr<Common::ShadowRenderer> VulkanRenderingFactory::CreateShadowRenderer() {
    return std::make_unique<VulkanShadowRenderer>();
}

std::unique_ptr<Common::SkyboxRenderer> VulkanRenderingFactory::CreateSkyboxRenderer() {
    return std::make_unique<VulkanSkyboxRenderer>();
}

std::unique_ptr<Common::PostProcessRenderer> VulkanRenderingFactory::CreatePostProcessRenderer() {
    return std::make_unique<VulkanPostProcessRenderer>();
}

} // namespace Lvs::Engine::Rendering::Vulkan
