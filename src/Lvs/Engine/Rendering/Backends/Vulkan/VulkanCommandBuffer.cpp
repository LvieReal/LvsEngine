#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanCommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"

namespace Lvs::Engine::Rendering::Backends::Vulkan {

VulkanCommandBuffer::VulkanCommandBuffer(VulkanContext& context, const VkCommandBuffer handle)
    : context_(context),
      handle_(handle) {}

VulkanCommandBuffer::~VulkanCommandBuffer() {
    context_.FreeCommandBuffer(handle_);
    handle_ = VK_NULL_HANDLE;
}

void VulkanCommandBuffer::BeginRenderPass(const RHI::RenderPassInfo& info) {
    context_.BeginRenderPass(handle_, info);
}

void VulkanCommandBuffer::EndRenderPass() {
    context_.EndRenderPass(handle_);
}

void VulkanCommandBuffer::BindPipeline(const RHI::IPipeline& pipeline) {
    context_.BindPipeline(handle_, pipeline);
}

void VulkanCommandBuffer::BindVertexBuffer(const RHI::u32 slot, const RHI::IBuffer& buffer, const std::size_t offset) {
    context_.BindVertexBuffer(handle_, slot, buffer, offset);
}

void VulkanCommandBuffer::BindIndexBuffer(const RHI::IBuffer& buffer, const RHI::IndexType indexType, const std::size_t offset) {
    context_.BindIndexBuffer(handle_, buffer, indexType, offset);
}

void VulkanCommandBuffer::BindResourceSet(const RHI::u32 slot, const RHI::IResourceSet& set) {
    context_.BindResourceSet(handle_, slot, set);
}

void VulkanCommandBuffer::PushConstants(const void* data, const std::size_t size) {
    context_.PushConstants(handle_, data, size);
}

void VulkanCommandBuffer::Draw(const RHI::u32 vertexCount) {
    context_.Draw(handle_, vertexCount);
}

void VulkanCommandBuffer::DrawIndexed(const RHI::u32 indexCount) {
    context_.DrawIndexed(handle_, indexCount);
}

VkCommandBuffer VulkanCommandBuffer::GetHandle() const {
    return handle_;
}

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
