#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Lvs::Engine::Rendering::Vulkan::BufferUtils {

struct BufferHandle {
    VkBuffer Buffer{VK_NULL_HANDLE};
    VkDeviceMemory Memory{VK_NULL_HANDLE};
};

std::uint32_t FindMemoryType(
    VkPhysicalDevice physicalDevice,
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties
);

BufferHandle CreateBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
);

void DestroyBuffer(VkDevice device, BufferHandle& buffer);

} // namespace Lvs::Engine::Rendering::Vulkan::BufferUtils
