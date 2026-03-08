#pragma once

#include "Lvs/Engine/RenderingV2/RHI/IContext.hpp"

namespace Lvs::Engine::RenderingV2::Backends::Vulkan {

namespace RHI = ::Lvs::Engine::RenderingV2::RHI;

struct VulkanApi {
    using BeginRenderPassFn = void (*)(const RHI::RenderPassInfo* info);
    using EndRenderPassFn = void (*)();
    using BindPipelineFn = void (*)(void* pipelineHandle);
    using BindResourceSetFn = void (*)(RHI::u32 slot, void* setHandle);
    using DrawIndexedFn = void (*)(RHI::u32 indexCount);
    using BindSampledImageFn = void (*)(RHI::u32 slot, void* imageViewHandle);

    BeginRenderPassFn BeginRenderPass{nullptr};
    EndRenderPassFn EndRenderPass{nullptr};
    BindPipelineFn BindPipeline{nullptr};
    BindResourceSetFn BindResourceSet{nullptr};
    DrawIndexedFn DrawIndexed{nullptr};
    BindSampledImageFn BindSampledImage{nullptr};
};

} // namespace Lvs::Engine::RenderingV2::Backends::Vulkan
