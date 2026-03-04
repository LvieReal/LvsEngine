#pragma once

#include <vulkan/vulkan.h>

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Rendering/Vulkan/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Engine::Rendering::Vulkan {

class Renderer;
class PostProcessRenderer;

class VulkanContext final {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    void Initialize();
    void AttachToNativeWindow(void* nativeWindowHandle, std::uint32_t width, std::uint32_t height);
    void Resize(std::uint32_t width, std::uint32_t height);
    void Render();
    void SetClearColor(float r, float g, float b, float a);
    void SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives);
    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();
    void Shutdown();

    [[nodiscard]] VkInstance GetInstance() const;
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const;
    [[nodiscard]] VkDevice GetDevice() const;
    [[nodiscard]] VkQueue GetGraphicsQueue() const;
    [[nodiscard]] std::uint32_t GetGraphicsQueueFamily() const;
    [[nodiscard]] VkRenderPass GetRenderPass() const;
    [[nodiscard]] VkRenderPass GetPostProcessRenderPass() const;
    [[nodiscard]] VkFormat GetSwapchainImageFormat() const;
    [[nodiscard]] VkExtent2D GetSwapchainExtent() const;
    [[nodiscard]] std::uint32_t GetFramesInFlight() const;

private:
    void CreateInstance();
    void CreateSurface(void* nativeWindowHandle);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain(std::uint32_t width, std::uint32_t height);
    void CreateImageViews();
    void CreateSceneRenderPass();
    void CreatePostProcessRenderPass();
    void CreateFramebuffers();
    void CreateOffscreenResources();
    void CreateDepthResources();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void RecreateSwapchain(std::uint32_t width, std::uint32_t height);
    void CleanupSwapchain();
    void CleanupDeviceAndSurface();
    void CleanupSyncObjects();
    bool IsPhysicalDeviceSuitable(VkPhysicalDevice device) const;

    std::uint32_t FindGraphicsPresentQueueFamily(VkPhysicalDevice device) const;

    VkInstance instance_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue graphicsQueue_{VK_NULL_HANDLE};
    std::uint32_t graphicsQueueFamily_{0};

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkFormat swapchainImageFormat_{VK_FORMAT_UNDEFINED};
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
    void* nativeWindowHandle_{nullptr};
    std::uint32_t lastWidth_{0};
    std::uint32_t lastHeight_{0};
    VkClearColorValue clearColor_{{1.0F, 1.0F, 1.0F, 1.0F}};
    std::vector<OverlayPrimitive> overlayPrimitives_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<PostProcessRenderer> postProcessRenderer_;
    std::shared_ptr<DataModel::Place> currentPlace_;
};

} // namespace Lvs::Engine::Rendering::Vulkan
