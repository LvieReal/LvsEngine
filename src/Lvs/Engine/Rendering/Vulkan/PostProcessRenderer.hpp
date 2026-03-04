#pragma once

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

class PostProcessRenderer final {
public:
    PostProcessRenderer() = default;
    ~PostProcessRenderer() = default;

    PostProcessRenderer(const PostProcessRenderer&) = delete;
    PostProcessRenderer& operator=(const PostProcessRenderer&) = delete;
    PostProcessRenderer(PostProcessRenderer&&) = delete;
    PostProcessRenderer& operator=(PostProcessRenderer&&) = delete;

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

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();

    void RecordBlurCommands(VulkanContext& context, VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void DrawComposite(VulkanContext& context, VkCommandBuffer commandBuffer, std::uint32_t imageIndex, std::uint32_t frameIndex);

private:
    struct BlurImageLevel {
        VkImage Image{VK_NULL_HANDLE};
        VkImageView View{VK_NULL_HANDLE};
        VkDeviceMemory Memory{VK_NULL_HANDLE};
        VkFramebuffer Framebuffer{VK_NULL_HANDLE};
    };

    void CreateCompositeDescriptorSetLayout(VulkanContext& context);
    void CreateBlurDescriptorSetLayout(VulkanContext& context);
    void CreatePipelineLayouts(VulkanContext& context);
    void CreateDescriptorPool(VulkanContext& context, std::uint32_t imageCount, std::uint32_t levelCount);
    void CreateBlurResources(VulkanContext& context, std::uint32_t imageCount);
    void CreateDescriptorSets(
        VulkanContext& context,
        const std::vector<VkImageView>& sceneViews,
        const std::vector<VkImageView>& glowViews
    );
    void CreateRenderPasses(VulkanContext& context);
    void CreatePipelines(VulkanContext& context);
    void DestroyBlurResources(VulkanContext& context);
    void DestroyPipelines(VulkanContext& context);
    [[nodiscard]] std::uint32_t ComputeUsedBlurLevels() const;
    [[nodiscard]] float GetBlurAmount() const;

    std::shared_ptr<DataModel::Lighting> GetLighting() const;

    static constexpr std::uint32_t MAX_BLUR_LEVELS = 4;

    std::shared_ptr<DataModel::Place> place_;
    VkDescriptorSetLayout compositeDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorSetLayout blurDescriptorSetLayout_{VK_NULL_HANDLE};
    VkPipelineLayout compositePipelineLayout_{VK_NULL_HANDLE};
    VkPipelineLayout blurPipelineLayout_{VK_NULL_HANDLE};
    VkPipeline compositePipeline_{VK_NULL_HANDLE};
    VkPipeline blurDownPipeline_{VK_NULL_HANDLE};
    VkPipeline blurUpPipeline_{VK_NULL_HANDLE};
    VkRenderPass blurRenderPass_{VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> compositeDescriptorSets_;
    std::vector<std::vector<VkDescriptorSet>> downDescriptorSets_;
    std::vector<std::vector<VkDescriptorSet>> upDescriptorSets_;
    std::vector<std::vector<BlurImageLevel>> downLevels_;
    std::vector<std::vector<BlurImageLevel>> upLevels_;
    std::vector<VkExtent2D> blurExtents_;
    VkSampler blurSampler_{VK_NULL_HANDLE};
    bool initialized_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
