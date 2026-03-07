#pragma once

#include "Lvs/Engine/Rendering/RenderingFactory.hpp"

#include <memory>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanRenderingFactory final : public RenderingFactory, public std::enable_shared_from_this<VulkanRenderingFactory> {
public:
    [[nodiscard]] RenderingApi GetApi() const override;
    [[nodiscard]] std::unique_ptr<Common::SceneRenderer> CreateSceneRenderer() override;
    [[nodiscard]] std::unique_ptr<Common::ShadowRenderer> CreateShadowRenderer() override;
    [[nodiscard]] std::unique_ptr<Common::SkyboxRenderer> CreateSkyboxRenderer() override;
    [[nodiscard]] std::unique_ptr<Common::PostProcessRenderer> CreatePostProcessRenderer() override;
};

} // namespace Lvs::Engine::Rendering::Vulkan
