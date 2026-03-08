#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderingFactory.hpp"

#include "Lvs/Engine/Rendering/Vulkan/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderer.hpp"

namespace Lvs::Engine::Rendering::Vulkan {

RenderingApi VulkanRenderingFactory::GetApi() const {
    return RenderingApi::Vulkan;
}

std::unique_ptr<Common::SceneRenderer> VulkanRenderingFactory::CreateSceneRenderer() {
    return std::make_unique<VulkanRenderer>(shared_from_this());
}

std::unique_ptr<Common::ShadowRenderer> VulkanRenderingFactory::CreateShadowRenderer() {
    return std::make_unique<ShadowRenderer>();
}

std::unique_ptr<Common::SkyboxRenderer> VulkanRenderingFactory::CreateSkyboxRenderer() {
    return std::make_unique<SkyboxRenderer>();
}

std::unique_ptr<Common::PostProcessRenderer> VulkanRenderingFactory::CreatePostProcessRenderer() {
    return std::make_unique<PostProcessRenderer>();
}

} // namespace Lvs::Engine::Rendering::Vulkan
