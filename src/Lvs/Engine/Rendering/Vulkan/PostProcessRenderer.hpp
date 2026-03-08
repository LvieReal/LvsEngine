#pragma once

#include "Lvs/Engine/Rendering/Common/BindingLayout.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/PipelineManifestProvider.hpp"
#include "Lvs/Engine/Rendering/Common/PostProcessSettingsResolver.hpp"
#include "Lvs/Engine/Rendering/Common/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSurface.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
class Lighting;
}

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanContext;
class VulkanPipelineLayout;
class VulkanPipelineVariant;

class PostProcessRenderer final : public Common::PostProcessRenderer {
public:
    PostProcessRenderer();
    ~PostProcessRenderer() override;

    PostProcessRenderer(const PostProcessRenderer&) = delete;
    PostProcessRenderer& operator=(const PostProcessRenderer&) = delete;
    PostProcessRenderer(PostProcessRenderer&&) = delete;
    PostProcessRenderer& operator=(PostProcessRenderer&&) = delete;

    void Initialize(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void RecreateSwapchain(Common::GraphicsContext& context, const Common::RenderSurface& surface) override;
    void DestroySwapchainResources(Common::GraphicsContext& context) override;
    void Shutdown(Common::GraphicsContext& context) override;

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place) override;
    void Unbind() override;

    void RecordBlurCommands(Common::GraphicsContext& context, Common::CommandBuffer& commandBuffer, std::uint32_t imageIndex) override;
    void DrawComposite(
        Common::GraphicsContext& context,
        Common::CommandBuffer& commandBuffer,
        std::uint32_t imageIndex,
        std::uint32_t frameIndex
    ) override;

private:
    struct BlurImageLevel {
        VkImage Image{VK_NULL_HANDLE};
        VkImageView View{VK_NULL_HANDLE};
        VkDeviceMemory Memory{VK_NULL_HANDLE};
        VkFramebuffer Framebuffer{VK_NULL_HANDLE};
    };

    void CreateCompositeBindingLayout(VulkanContext& context, std::uint32_t imageCount, std::uint32_t levelCount);
    void CreateBlurBindingLayout(VulkanContext& context, std::uint32_t imageCount, std::uint32_t levelCount);
    void CreatePipelineLayouts(VulkanContext& context);
    void CreateBlurResources(VulkanContext& context, std::uint32_t imageCount);
    void CreateBindings(
        VulkanContext& context,
        const std::vector<VkImageView>& sceneViews,
        const std::vector<VkImageView>& glowViews
    );
    void CreateRenderPasses(VulkanContext& context);
    void CreatePipelines(VulkanContext& context);
    void Initialize(
        VulkanContext& context,
        const std::vector<VkImageView>& sceneViews,
        const std::vector<VkImageView>& glowViews,
        VkSampler sourceSampler
    );
    void RecreateSwapchain(
        VulkanContext& context,
        const std::vector<VkImageView>& sceneViews,
        const std::vector<VkImageView>& glowViews,
        VkSampler sourceSampler
    );
    void DestroySwapchainResources(VulkanContext& context);
    void Shutdown(VulkanContext& context);
    void DestroyBlurResources(VulkanContext& context);
    void DestroyPipelines(VulkanContext& context);
    [[nodiscard]] std::uint32_t ComputeUsedBlurLevels() const;
    [[nodiscard]] float GetBlurAmount() const;

    std::shared_ptr<DataModel::Place> place_;
    std::unique_ptr<Common::BindingLayout> compositeBindingLayout_;
    std::unique_ptr<Common::BindingLayout> blurBindingLayout_;
    std::unique_ptr<VulkanPipelineLayout> compositePipelineLayout_;
    std::unique_ptr<VulkanPipelineLayout> blurPipelineLayout_;
    std::unique_ptr<VulkanPipelineVariant> compositePipeline_;
    std::unique_ptr<VulkanPipelineVariant> blurDownPipeline_;
    std::unique_ptr<VulkanPipelineVariant> blurUpPipeline_;
    const Common::RenderPass* compositeRenderPass_{nullptr};
    VkRenderPass blurRenderPass_{VK_NULL_HANDLE};
    std::vector<std::unique_ptr<Common::ResourceBinding>> compositeBindings_;
    std::vector<std::vector<std::unique_ptr<Common::ResourceBinding>>> downBindings_;
    std::vector<std::vector<std::unique_ptr<Common::ResourceBinding>>> upBindings_;
    std::vector<std::vector<BlurImageLevel>> downLevels_;
    std::vector<std::vector<BlurImageLevel>> upLevels_;
    std::vector<VkExtent2D> blurExtents_;
    VkSampler blurSampler_{VK_NULL_HANDLE};
    std::shared_ptr<Common::PipelineManifestProvider> pipelineManifest_;
    Common::PostProcessSettingsResolver settingsResolver_{};
    Common::PostProcessSettingsSnapshot settingsSnapshot_{};
    bool initialized_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
