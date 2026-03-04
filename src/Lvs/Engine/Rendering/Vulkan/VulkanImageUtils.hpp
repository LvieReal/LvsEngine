#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Lvs::Engine::Rendering::Vulkan::ImageUtils {

struct ImageHandle {
    VkImage Image{VK_NULL_HANDLE};
    VkDeviceMemory Memory{VK_NULL_HANDLE};
};

ImageHandle CreateImage2D(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t mipLevels,
    std::uint32_t layers,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags = 0
);

VkImageView CreateImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectMask,
    VkImageViewType viewType,
    std::uint32_t mipLevels,
    std::uint32_t layers
);

VkSampler CreateSampler(
    VkDevice device,
    VkFilter minMagFilter,
    VkSamplerAddressMode addressMode,
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR
);

void DestroyImage(VkDevice device, ImageHandle& image);

} // namespace Lvs::Engine::Rendering::Vulkan::ImageUtils
