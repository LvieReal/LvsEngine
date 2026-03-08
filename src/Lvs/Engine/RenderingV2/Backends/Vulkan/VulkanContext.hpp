#pragma once

#include "Lvs/Engine/RenderingV2/Backends/Vulkan/VulkanApi.hpp"
#include "Lvs/Engine/RenderingV2/RHI/IContext.hpp"

namespace Lvs::Engine::RenderingV2::Backends::Vulkan {

class VulkanContext final : public RHI::IContext {
public:
    explicit VulkanContext(VulkanApi api);
    std::unique_ptr<RHI::ICommandBuffer> AllocateCommandBuffer() override;
    std::unique_ptr<RHI::IPipeline> CreatePipeline(const RHI::PipelineDesc& desc) override;
    void BindTexture(RHI::u32 slot, const RHI::Texture& texture) override;

private:
    VulkanApi api_;
};

} // namespace Lvs::Engine::RenderingV2::Backends::Vulkan
