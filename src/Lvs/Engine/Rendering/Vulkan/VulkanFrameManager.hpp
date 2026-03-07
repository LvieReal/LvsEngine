#pragma once

#include "Lvs/Engine/Rendering/Common/RenderSurface.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

class VulkanContext;

class VulkanFrameManager final : public Common::RenderSurface {
public:
    struct FrameState {
        VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
        std::uint32_t ImageIndex{0};
        std::uint32_t FrameIndex{0};
    };

    VulkanFrameManager() = default;
    ~VulkanFrameManager() = default;

    VulkanFrameManager(const VulkanFrameManager&) = delete;
    VulkanFrameManager& operator=(const VulkanFrameManager&) = delete;

    void Recreate(VulkanContext& context, VkSurfaceKHR surface, std::uint32_t width, std::uint32_t height);
    void CleanupSwapchain(VulkanContext& context);
    void CleanupSyncObjects(VulkanContext& context);
    void Cleanup(VulkanContext& context);

    [[nodiscard]] bool HasSwapchain() const;
    [[nodiscard]] FrameState BeginFrame(VulkanContext& context, bool& needsRecreate);
    [[nodiscard]] bool EndFrame(VulkanContext& context, const FrameState& frameState);

    [[nodiscard]] VkRenderPass GetSceneRenderPass() const;
    [[nodiscard]] VkRenderPass GetPostProcessRenderPass() const;
    [[nodiscard]] VkFormat GetSwapchainImageFormat() const;
    [[nodiscard]] VkFormat GetOffscreenImageFormat() const;
    [[nodiscard]] VkExtent2D GetSwapchainExtentVk() const;
    [[nodiscard]] Common::Extent2D GetExtent() const override;
    [[nodiscard]] std::uint32_t GetFramesInFlight() const override;
    [[nodiscard]] VkFramebuffer GetSceneFramebuffer(std::uint32_t imageIndex) const;
    [[nodiscard]] VkFramebuffer GetSwapchainFramebuffer(std::uint32_t imageIndex) const;
    [[nodiscard]] VkImage GetOffscreenColorImage(std::uint32_t imageIndex) const;
    [[nodiscard]] VkImage GetOffscreenGlowImage(std::uint32_t imageIndex) const;
    [[nodiscard]] const std::vector<VkImageView>& GetOffscreenColorImageViews() const;
    [[nodiscard]] const std::vector<VkImageView>& GetOffscreenGlowImageViews() const;
    [[nodiscard]] VkSampler GetOffscreenColorSampler() const;

private:
    void CreateSwapchain(VulkanContext& context, VkSurfaceKHR surface, std::uint32_t width, std::uint32_t height);
    void CreateImageViews(VulkanContext& context);
    void SelectOffscreenImageFormat(VulkanContext& context);
    void CreateSceneRenderPass(VulkanContext& context);
    void CreatePostProcessRenderPass(VulkanContext& context);
    void CreateFramebuffers(VulkanContext& context);
    void CreateOffscreenResources(VulkanContext& context);
    void CreateDepthResources(VulkanContext& context);
    void CreateCommandPool(VulkanContext& context);
    void CreateCommandBuffers(VulkanContext& context);
    void CreateSyncObjects(VulkanContext& context);

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkFormat swapchainImageFormat_{VK_FORMAT_UNDEFINED};
    VkFormat offscreenImageFormat_{VK_FORMAT_UNDEFINED};
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass sceneRenderPass_{VK_NULL_HANDLE};
    VkRenderPass postProcessRenderPass_{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> sceneFramebuffers_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    std::vector<VkImage> offscreenColorImages_;
    std::vector<VkImage> offscreenGlowImages_;
    std::vector<VkImageView> offscreenColorImageViews_;
    std::vector<VkImageView> offscreenGlowImageViews_;
    std::vector<VkDeviceMemory> offscreenColorMemories_;
    std::vector<VkDeviceMemory> offscreenGlowMemories_;
    VkSampler offscreenColorSampler_{VK_NULL_HANDLE};
    std::vector<VkImage> depthImages_;
    std::vector<VkImageView> depthImageViews_;
    std::vector<VkDeviceMemory> depthMemories_;
    VkFormat depthFormat_{VK_FORMAT_UNDEFINED};
    VkCommandPool commandPool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers_;

    static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::uint32_t currentFrame_{0};
    bool syncObjectsCreated_{false};
};

} // namespace Lvs::Engine::Rendering::Vulkan
