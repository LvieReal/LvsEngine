#pragma once

#include "Lvs/Engine/Rendering/RHI/ICommandBuffer.hpp"

#include <vulkan/vulkan.h>

namespace Lvs::Engine::Rendering::Backends::Vulkan {

class VulkanContext;

class VulkanCommandBuffer final : public RHI::ICommandBuffer {
public:
    VulkanCommandBuffer(VulkanContext& context, VkCommandBuffer handle);
    ~VulkanCommandBuffer() override;

    void BeginRenderPass(const RHI::RenderPassInfo& info) override;
    void EndRenderPass() override;
    void BindPipeline(const RHI::IPipeline& pipeline) override;
    void BindVertexBuffer(RHI::u32 slot, const RHI::IBuffer& buffer, std::size_t offset) override;
    void BindIndexBuffer(const RHI::IBuffer& buffer, RHI::IndexType indexType, std::size_t offset) override;
    void BindResourceSet(RHI::u32 slot, const RHI::IResourceSet& set) override;
    void PushConstants(const void* data, std::size_t size) override;
    void Draw(const RHI::ICommandBuffer::DrawInfo& info) override;
    [[nodiscard]] VkCommandBuffer GetHandle() const;

private:
    VulkanContext& context_;
    VkCommandBuffer handle_{VK_NULL_HANDLE};
};

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
