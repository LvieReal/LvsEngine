#pragma once

#include "Lvs/Engine/Rendering/RHI/IBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/IPipeline.hpp"
#include "Lvs/Engine/Rendering/RHI/IResourceSet.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

namespace Lvs::Engine::Rendering::RHI {

struct RenderPassInfo {
    u32 width{0};
    u32 height{0};
    void* renderPassHandle{nullptr};
    void* framebufferHandle{nullptr};
    bool clearColor{false};
    float clearColorValue[4]{0.0F, 0.0F, 0.0F, 1.0F};
    bool clearDepth{false};
    float clearDepthValue{0.0F};
};

class ICommandBuffer {
public:
    virtual ~ICommandBuffer() = default;
    virtual void BeginRenderPass(const RenderPassInfo& info) = 0;
    virtual void EndRenderPass() = 0;
    virtual void BindPipeline(const IPipeline& pipeline) = 0;
    virtual void BindVertexBuffer(u32 slot, const IBuffer& buffer, std::size_t offset) = 0;
    virtual void BindIndexBuffer(const IBuffer& buffer, IndexType indexType, std::size_t offset) = 0;
    virtual void BindResourceSet(u32 slot, const IResourceSet& set) = 0;
    virtual void PushConstants(const void* data, std::size_t size) = 0;
    virtual void DrawIndexed(u32 indexCount) = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
