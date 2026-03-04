#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

class QString;

namespace Lvs::Engine::Rendering::Vulkan::TextureUtils {

struct Texture2DHandle {
    VkImage Image{VK_NULL_HANDLE};
    VkDeviceMemory Memory{VK_NULL_HANDLE};
    VkImageView View{VK_NULL_HANDLE};
    VkSampler Sampler{VK_NULL_HANDLE};
    std::uint32_t Width{0};
    std::uint32_t Height{0};
};

struct CubemapHandle {
    VkImage Image{VK_NULL_HANDLE};
    VkDeviceMemory Memory{VK_NULL_HANDLE};
    VkImageView View{VK_NULL_HANDLE};
    VkSampler Sampler{VK_NULL_HANDLE};
    std::uint32_t Width{0};
    std::uint32_t Height{0};
};

CubemapHandle CreateCubemapFromPaths(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue queue,
    std::uint32_t queueFamilyIndex,
    const std::array<QString, 6>& facePaths,
    bool linearFiltering
);

Texture2DHandle CreateTexture2DFromPath(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue queue,
    std::uint32_t queueFamilyIndex,
    const QString& imagePath,
    bool linearFiltering,
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
);

void DestroyTexture2D(VkDevice device, Texture2DHandle& texture);

void DestroyCubemap(VkDevice device, CubemapHandle& cubemap);

} // namespace Lvs::Engine::Rendering::Vulkan::TextureUtils
