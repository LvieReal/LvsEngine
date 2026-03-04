#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"

#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan::BufferUtils {

std::uint32_t FindMemoryType(
    const VkPhysicalDevice physicalDevice,
    const std::uint32_t typeFilter,
    const VkMemoryPropertyFlags properties
) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1U << i)) == 0U) {
            continue;
        }
        if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable Vulkan memory type.");
}

BufferHandle CreateBuffer(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties
) {
    BufferHandle handle;

    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &handle.Buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan buffer.");
    }

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(device, handle.Buffer, &memRequirements);

    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    if (vkAllocateMemory(device, &allocInfo, nullptr, &handle.Memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, handle.Buffer, nullptr);
        handle.Buffer = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to allocate Vulkan buffer memory.");
    }

    vkBindBufferMemory(device, handle.Buffer, handle.Memory, 0);
    return handle;
}

void DestroyBuffer(const VkDevice device, BufferHandle& buffer) {
    if (buffer.Buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer.Buffer, nullptr);
        buffer.Buffer = VK_NULL_HANDLE;
    }
    if (buffer.Memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer.Memory, nullptr);
        buffer.Memory = VK_NULL_HANDLE;
    }
}

} // namespace Lvs::Engine::Rendering::Vulkan::BufferUtils
