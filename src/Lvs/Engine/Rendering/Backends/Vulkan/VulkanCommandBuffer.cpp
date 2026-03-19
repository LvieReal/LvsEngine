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

void VulkanCommandBuffer::PushConstants(const PushConstantsInfo& info) {
    context_.PushConstants(handle_, info);
}

void VulkanCommandBuffer::Draw(const RHI::ICommandBuffer::DrawInfo& info) {
    context_.Draw(handle_, info);
}

VkCommandBuffer VulkanCommandBuffer::GetHandle() const {
    return handle_;
}

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
