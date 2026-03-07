#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"

#include <cstring>
#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan {

VulkanBufferResource::VulkanBufferResource(const VkDevice device, const VkBuffer buffer, const VkDeviceMemory memory, const std::size_t size)
    : device_(device),
      buffer_(buffer),
      memory_(memory),
      size_(size) {
}

VulkanBufferResource::~VulkanBufferResource() {
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
}

std::size_t VulkanBufferResource::GetSize() const {
    return size_;
}

void VulkanBufferResource::Upload(const void* data, const std::size_t size, const std::size_t offset) {
    if (size == 0) {
        return;
    }
    if (offset + size > size_) {
        throw std::runtime_error("Buffer upload exceeds allocated range.");
    }

    void* mapped = nullptr;
    vkMapMemory(device_, memory_, static_cast<VkDeviceSize>(offset), static_cast<VkDeviceSize>(size), 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(device_, memory_);
}

VkBuffer VulkanBufferResource::GetBuffer() const {
    return buffer_;
}

VulkanImageResource::VulkanImageResource(
    const VkDevice device,
    const VkImage image,
    const VkDeviceMemory memory,
    const std::uint32_t width,
    const std::uint32_t height
) : device_(device),
    image_(image),
    memory_(memory),
    width_(width),
    height_(height) {
}

VulkanImageResource::~VulkanImageResource() {
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
}

std::uint32_t VulkanImageResource::GetWidth() const {
    return width_;
}

std::uint32_t VulkanImageResource::GetHeight() const {
    return height_;
}

VkImage VulkanImageResource::GetImage() const {
    return image_;
}

VkDeviceMemory VulkanImageResource::GetMemory() const {
    return memory_;
}

VulkanSamplerResource::VulkanSamplerResource(const VkDevice device, const VkSampler sampler)
    : device_(device),
      sampler_(sampler) {
}

VulkanSamplerResource::~VulkanSamplerResource() {
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler_, nullptr);
    }
}

VkSampler VulkanSamplerResource::GetSampler() const {
    return sampler_;
}

VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(const VkCommandBuffer commandBuffer)
    : commandBuffer_(commandBuffer) {
}

void VulkanRenderCommandBuffer::BindVertexBuffer(
    const Common::BufferResource& buffer,
    const std::uint32_t binding,
    const std::size_t offset
) {
    const auto* vkBuffer = dynamic_cast<const VulkanBufferResource*>(&buffer);
    if (vkBuffer == nullptr) {
        throw std::runtime_error("CommandBuffer received a non-Vulkan vertex buffer.");
    }
    const VkBuffer vertexBuffers[] = {vkBuffer->GetBuffer()};
    const VkDeviceSize offsets[] = {static_cast<VkDeviceSize>(offset)};
    vkCmdBindVertexBuffers(commandBuffer_, binding, 1, vertexBuffers, offsets);
}

void VulkanRenderCommandBuffer::BindIndexBuffer(
    const Common::BufferResource& buffer,
    const Common::IndexFormat format,
    const std::size_t offset
) {
    const auto* vkBuffer = dynamic_cast<const VulkanBufferResource*>(&buffer);
    if (vkBuffer == nullptr) {
        throw std::runtime_error("CommandBuffer received a non-Vulkan index buffer.");
    }
    const VkIndexType indexType = format == Common::IndexFormat::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(commandBuffer_, vkBuffer->GetBuffer(), static_cast<VkDeviceSize>(offset), indexType);
}

void VulkanRenderCommandBuffer::DrawIndexed(
    const std::uint32_t indexCount,
    const std::uint32_t instanceCount,
    const std::uint32_t firstIndex
) {
    vkCmdDrawIndexed(commandBuffer_, indexCount, instanceCount, firstIndex, 0, 0);
}

VkCommandBuffer VulkanRenderCommandBuffer::GetHandle() const {
    return commandBuffer_;
}

} // namespace Lvs::Engine::Rendering::Vulkan
