#pragma once

#include "Lvs/Engine/Rendering/RHI/ICommandBuffer.hpp"

namespace Lvs::Engine::Rendering::Backends::OpenGL {

class GLContext;

class GLCommandBuffer final : public RHI::ICommandBuffer {
public:
    explicit GLCommandBuffer(GLContext& context);
    ~GLCommandBuffer() override = default;

    void BeginRenderPass(const RHI::RenderPassInfo& info) override;
    void EndRenderPass() override;
    void BindPipeline(const RHI::IPipeline& pipeline) override;
    void BindVertexBuffer(RHI::u32 slot, const RHI::IBuffer& buffer, std::size_t offset) override;
    void BindIndexBuffer(const RHI::IBuffer& buffer, RHI::IndexType indexType, std::size_t offset) override;
    void BindResourceSet(RHI::u32 slot, const RHI::IResourceSet& set) override;
    void PushConstants(const void* data, std::size_t size) override;
    void DrawIndexed(RHI::u32 indexCount) override;

private:
    GLContext& context_;
};

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
