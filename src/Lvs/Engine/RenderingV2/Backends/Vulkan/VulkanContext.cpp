#include "Lvs/Engine/RenderingV2/Backends/Vulkan/VulkanContext.hpp"

namespace Lvs::Engine::RenderingV2::Backends::Vulkan {

namespace {

class VulkanPipeline final : public RHI::IPipeline {
public:
    explicit VulkanPipeline(const RHI::PipelineDesc& desc)
        : desc_(desc) {}

    [[nodiscard]] void* GetHandle() const {
        return nullptr;
    }

private:
    RHI::PipelineDesc desc_{};
};

class VulkanCommandBuffer final : public RHI::ICommandBuffer {
public:
    explicit VulkanCommandBuffer(VulkanApi api)
        : api_(api) {}

    void BeginRenderPass(const RHI::RenderPassInfo& info) override {
        if (api_.BeginRenderPass != nullptr) {
            api_.BeginRenderPass(&info);
        }
    }

    void EndRenderPass() override {
        if (api_.EndRenderPass != nullptr) {
            api_.EndRenderPass();
        }
    }

    void BindPipeline(const RHI::IPipeline& pipeline) override {
        const auto* vkPipeline = dynamic_cast<const VulkanPipeline*>(&pipeline);
        if (vkPipeline == nullptr || api_.BindPipeline == nullptr) {
            return;
        }
        api_.BindPipeline(vkPipeline->GetHandle());
    }

    void BindResourceSet(const RHI::u32 slot, const RHI::ResourceSet& set) override {
        if (api_.BindResourceSet == nullptr) {
            return;
        }
        api_.BindResourceSet(slot, set.native_handle_ptr);
    }

    void DrawIndexed(const RHI::u32 indexCount) override {
        if (api_.DrawIndexed != nullptr) {
            api_.DrawIndexed(indexCount);
        }
    }

private:
    VulkanApi api_{};
};

} // namespace

VulkanContext::VulkanContext(VulkanApi api)
    : api_(api) {}

std::unique_ptr<RHI::ICommandBuffer> VulkanContext::AllocateCommandBuffer() {
    return std::make_unique<VulkanCommandBuffer>(api_);
}

std::unique_ptr<RHI::IPipeline> VulkanContext::CreatePipeline(const RHI::PipelineDesc& desc) {
    return std::make_unique<VulkanPipeline>(desc);
}

void VulkanContext::BindTexture(const RHI::u32 slot, const RHI::Texture& texture) {
    if (api_.BindSampledImage == nullptr) {
        return;
    }
    api_.BindSampledImage(slot, texture.graphic_handle_ptr);
}

} // namespace Lvs::Engine::RenderingV2::Backends::Vulkan
