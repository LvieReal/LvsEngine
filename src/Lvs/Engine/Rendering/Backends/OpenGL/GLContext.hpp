#pragma once

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLApi.hpp"
#include "Lvs/Engine/Rendering/Renderer.hpp"
#include "Lvs/Engine/Rendering/RHI/IContext.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::OpenGL {

class GLCommandBuffer;

class GLContext final : public RHI::IContext {
public:
    explicit GLContext(GLApi api);
    ~GLContext() override;
    std::unique_ptr<RHI::ICommandBuffer> AllocateCommandBuffer() override;
    std::unique_ptr<RHI::IPipeline> CreatePipeline(const RHI::PipelineDesc& desc) override;
    std::unique_ptr<RHI::IBuffer> CreateBuffer(const RHI::BufferDesc& desc) override;
    std::unique_ptr<RHI::IResourceSet> CreateResourceSet(const RHI::ResourceSetDesc& desc) override;
    [[nodiscard]] RHI::Texture CreateTextureCube(const RHI::CubemapDesc& desc) override;
    void DestroyTexture(RHI::Texture& texture) override;
    void BindTexture(RHI::u32 slot, const RHI::Texture& texture) override;
    [[nodiscard]] void* GetDefaultRenderPassHandle() const override;
    [[nodiscard]] void* GetDefaultFramebufferHandle() const override;

    void Initialize(RHI::u32 width, RHI::u32 height);
    void Render(const ::Lvs::Engine::Rendering::SceneData& sceneData);
    void WaitIdle();

    void BeginRenderPass(const RHI::RenderPassInfo& info);
    void EndRenderPass();
    void BindPipeline(const RHI::IPipeline& pipeline);
    void BindVertexBuffer(RHI::u32 slot, const RHI::IBuffer& buffer, std::size_t offset);
    void BindIndexBuffer(const RHI::IBuffer& buffer, RHI::IndexType indexType, std::size_t offset);
    void BindResourceSet(RHI::u32 slot, const RHI::IResourceSet& set);
    void PushConstants(const void* data, std::size_t size);
    void DrawIndexed(RHI::u32 indexCount);

private:
    bool EnsureNativeContext();
    void DestroyNativeContext();

    GLApi api_;
    std::unique_ptr<::Lvs::Engine::Rendering::Renderer> renderer_;
    std::unique_ptr<GLCommandBuffer> cmdBuffer_;
    RHI::u32 frameIndex_{0};
    unsigned int defaultVao_{0U};
    unsigned int pushConstantBuffer_{0U};
    unsigned int currentProgram_{0U};
    void* deviceContext_{nullptr};
    RHI::IndexType currentIndexType_{RHI::IndexType::UInt32};
    RHI::VertexLayout currentVertexLayout_{RHI::VertexLayout::None};
    std::unordered_map<RHI::u32, RHI::Texture> textureSlots_;
    std::unordered_map<RHI::u32, std::vector<RHI::ResourceBinding>> resourceSetTextures_;
    std::unordered_map<unsigned int, RHI::TextureType> ownedTextures_;
};

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
