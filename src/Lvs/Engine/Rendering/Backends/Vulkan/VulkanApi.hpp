#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::Vulkan {

struct VulkanApi {
    VkInstance Instance{VK_NULL_HANDLE};
    VkPhysicalDevice PhysicalDevice{VK_NULL_HANDLE};
    VkDevice Device{VK_NULL_HANDLE};
    VkQueue GraphicsQueue{VK_NULL_HANDLE};
    VkQueue PresentQueue{VK_NULL_HANDLE};
    VkCommandPool CommandPool{VK_NULL_HANDLE};
    VkSurfaceKHR Surface{VK_NULL_HANDLE};
    VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
    std::vector<VkImage> SwapchainImages{};
    std::vector<VkImageView> SwapchainImageViews{};
    std::vector<VkFramebuffer> SwapchainFramebuffers{};
    void* NativeWindowHandle{nullptr};

    VkFormat ColorFormat{VK_FORMAT_B8G8R8A8_UNORM};
    VkFormat DepthFormat{VK_FORMAT_D32_SFLOAT};
    VkImageView ColorAttachmentView{VK_NULL_HANDLE};
    VkImageView DepthAttachmentView{VK_NULL_HANDLE};

    VkRenderPass RenderPass{VK_NULL_HANDLE};
    VkFramebuffer Framebuffer{VK_NULL_HANDLE};
    VkPipelineLayout PipelineLayout{VK_NULL_HANDLE};

    VkDescriptorPool DescriptorPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout DescriptorSetLayout{VK_NULL_HANDLE};
    VkSampler DefaultSampler{VK_NULL_HANDLE};

    VkExtent2D SurfaceExtent{0U, 0U};
    std::uint32_t NegotiatedApiVersion{VK_API_VERSION_1_0};
};

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
