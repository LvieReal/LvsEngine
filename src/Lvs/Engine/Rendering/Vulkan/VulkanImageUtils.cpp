#include "Lvs/Engine/Rendering/Vulkan/VulkanImageUtils.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"

#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan::ImageUtils {

ImageHandle CreateImage2D(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t mipLevels,
    const std::uint32_t layers,
    const VkFormat format,
    const VkImageTiling tiling,
    const VkImageUsageFlags usage,
    const VkImageCreateFlags flags
) {
    ImageHandle image{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = mipLevels,
        .arrayLayers = layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &imageInfo, nullptr, &image.Image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image.");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, image.Image, &memRequirements);
    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = BufferUtils::FindMemoryType(
            physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &image.Memory) != VK_SUCCESS) {
        vkDestroyImage(device, image.Image, nullptr);
        image.Image = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to allocate image memory.");
    }
    vkBindImageMemory(device, image.Image, image.Memory, 0);
    return image;
}

VkImageView CreateImageView(
    const VkDevice device,
    const VkImage image,
    const VkFormat format,
    const VkImageAspectFlags aspectMask,
    const VkImageViewType viewType,
    const std::uint32_t mipLevels,
    const std::uint32_t layers
) {
    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = viewType,
        .format = format,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = aspectMask,
                             .baseMipLevel = 0,
                             .levelCount = mipLevels,
                             .baseArrayLayer = 0,
                             .layerCount = layers}
    };

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view.");
    }
    return view;
}

VkSampler CreateSampler(
    const VkDevice device,
    const VkFilter minMagFilter,
    const VkSamplerAddressMode addressMode,
    const VkSamplerMipmapMode mipmapMode
) {
    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = minMagFilter,
        .minFilter = minMagFilter,
        .mipmapMode = mipmapMode,
        .addressModeU = addressMode,
        .addressModeV = addressMode,
        .addressModeW = addressMode,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler.");
    }
    return sampler;
}

void DestroyImage(const VkDevice device, ImageHandle& image) {
    if (image.Image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image.Image, nullptr);
        image.Image = VK_NULL_HANDLE;
    }
    if (image.Memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image.Memory, nullptr);
        image.Memory = VK_NULL_HANDLE;
    }
}

} // namespace Lvs::Engine::Rendering::Vulkan::ImageUtils
