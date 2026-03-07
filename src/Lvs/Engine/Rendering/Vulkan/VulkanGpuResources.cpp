#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"

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

void* VulkanBufferResource::GetNativeHandle() const {
    return reinterpret_cast<void*>(buffer_);
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

void* VulkanImageResource::GetNativeHandle() const {
    return reinterpret_cast<void*>(image_);
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

void* VulkanSamplerResource::GetNativeHandle() const {
    return reinterpret_cast<void*>(sampler_);
}

VkSampler VulkanSamplerResource::GetSampler() const {
    return sampler_;
}

VulkanDrawPassState::VulkanDrawPassState(
    const VkRenderPass renderPass,
    const VkFramebuffer framebuffer,
    const Common::Rect renderArea,
    const VkClearValue* clearValues,
    const std::uint32_t clearValueCount
) {
    beginInfo_ = VkRenderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {renderArea.X, renderArea.Y},
            .extent = {renderArea.Width, renderArea.Height}
        },
        .clearValueCount = clearValueCount,
        .pClearValues = clearValues
    };
}

const VkRenderPassBeginInfo& VulkanDrawPassState::GetBeginInfo() const {
    return beginInfo_;
}

VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(const VkCommandBuffer commandBuffer)
    : commandBuffer_(commandBuffer) {
}

void* VulkanRenderCommandBuffer::GetNativeHandle() const {
    return commandBuffer_;
}

void VulkanRenderCommandBuffer::BeginDrawPass(const Common::DrawPassState& state) {
    const auto& vkState = static_cast<const VulkanDrawPassState&>(state);
    vkCmdBeginRenderPass(commandBuffer_, &vkState.GetBeginInfo(), VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderCommandBuffer::EndDrawPass() {
    vkCmdEndRenderPass(commandBuffer_);
}

void VulkanRenderCommandBuffer::BindVertexBuffer(
    const Common::BufferResource& buffer,
    const std::uint32_t binding,
    const std::size_t offset
) {
    const auto vkBuffer = reinterpret_cast<VkBuffer>(buffer.GetNativeHandle());
    if (vkBuffer == VK_NULL_HANDLE) {
        throw std::runtime_error("CommandBuffer received an invalid native vertex buffer.");
    }
    const VkBuffer vertexBuffers[] = {vkBuffer};
    const VkDeviceSize offsets[] = {static_cast<VkDeviceSize>(offset)};
    vkCmdBindVertexBuffers(commandBuffer_, binding, 1, vertexBuffers, offsets);
}

void VulkanRenderCommandBuffer::BindIndexBuffer(
    const Common::BufferResource& buffer,
    const Common::IndexFormat format,
    const std::size_t offset
) {
    const auto vkBuffer = reinterpret_cast<VkBuffer>(buffer.GetNativeHandle());
    if (vkBuffer == VK_NULL_HANDLE) {
        throw std::runtime_error("CommandBuffer received an invalid native index buffer.");
    }
    const VkIndexType indexType = format == Common::IndexFormat::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(commandBuffer_, vkBuffer, static_cast<VkDeviceSize>(offset), indexType);
}

void VulkanRenderCommandBuffer::SetViewport(const Common::Viewport& viewport) {
    const VkViewport vkViewport{
        .x = viewport.X,
        .y = viewport.Y,
        .width = viewport.Width,
        .height = viewport.Height,
        .minDepth = viewport.MinDepth,
        .maxDepth = viewport.MaxDepth
    };
    vkCmdSetViewport(commandBuffer_, 0, 1, &vkViewport);
}

void VulkanRenderCommandBuffer::SetScissor(const Common::Rect& scissor) {
    const VkRect2D vkScissor{
        .offset = {scissor.X, scissor.Y},
        .extent = {scissor.Width, scissor.Height}
    };
    vkCmdSetScissor(commandBuffer_, 0, 1, &vkScissor);
}

void VulkanRenderCommandBuffer::PushConstants(
    const Common::PipelineLayout& layout,
    const Common::ShaderStageFlags stages,
    const void* data,
    const std::size_t size,
    const std::uint32_t offset
) {
    const auto vkPipelineLayout = reinterpret_cast<VkPipelineLayout>(layout.GetNativeHandle());
    VkShaderStageFlags stageFlags = 0;
    if ((static_cast<std::uint32_t>(stages) & static_cast<std::uint32_t>(Common::ShaderStageFlags::Vertex)) != 0U) {
        stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if ((static_cast<std::uint32_t>(stages) & static_cast<std::uint32_t>(Common::ShaderStageFlags::Fragment)) != 0U) {
        stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    vkCmdPushConstants(commandBuffer_, vkPipelineLayout, stageFlags, offset, static_cast<std::uint32_t>(size), data);
}

void VulkanRenderCommandBuffer::Draw(
    const std::uint32_t vertexCount,
    const std::uint32_t instanceCount,
    const std::uint32_t firstVertex
) {
    vkCmdDraw(commandBuffer_, vertexCount, instanceCount, firstVertex, 0);
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
