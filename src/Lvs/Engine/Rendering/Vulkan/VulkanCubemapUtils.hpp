#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <filesystem>

namespace Lvs::Engine::Rendering::Vulkan::CubemapUtils {

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
    const std::array<std::filesystem::path, 6>& facePaths,
    bool linearFiltering,
    int resolutionCap,
    bool compression
);

CubemapHandle CreateCubemapFromCrossPath(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue queue,
    std::uint32_t queueFamilyIndex,
    const std::filesystem::path& crossPath,
    bool linearFiltering,
    int resolutionCap,
    bool compression
);

void DestroyCubemap(VkDevice device, CubemapHandle& cubemap);

} // namespace Lvs::Engine::Rendering::Vulkan::CubemapUtils
