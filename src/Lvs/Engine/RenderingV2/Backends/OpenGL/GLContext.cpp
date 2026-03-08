#include "Lvs/Engine/RenderingV2/Backends/OpenGL/GLContext.hpp"

namespace Lvs::Engine::RenderingV2::Backends::OpenGL {

namespace {

class GLPipeline final : public RHI::IPipeline {
public:
    explicit GLPipeline(const RHI::PipelineDesc& desc)
        : desc_(desc) {}

    [[nodiscard]] unsigned int GetProgramHandle() const {
        return 0U;
    }

private:
    RHI::PipelineDesc desc_{};
};

class GLCommandBuffer final : public RHI::ICommandBuffer {
public:
    explicit GLCommandBuffer(GLApi api)
        : api_(api) {}

    void BeginRenderPass(const RHI::RenderPassInfo& info) override {
        currentPass_ = info;
    }

    void EndRenderPass() override {}

    void BindPipeline(const RHI::IPipeline& pipeline) override {
        const auto* glPipeline = dynamic_cast<const GLPipeline*>(&pipeline);
        if (glPipeline == nullptr || api_.UseProgram == nullptr) {
            return;
        }
        api_.UseProgram(glPipeline->GetProgramHandle());
    }

    void BindResourceSet(const RHI::u32 slot, const RHI::ResourceSet& set) override {
        static_cast<void>(slot);
        static_cast<void>(set);
    }

    void DrawIndexed(const RHI::u32 indexCount) override {
        if (api_.DrawElements == nullptr || indexCount == 0) {
            return;
        }
        api_.DrawElements(api_.TrianglesEnum, static_cast<int>(indexCount), api_.UnsignedIntEnum, nullptr);
    }

private:
    GLApi api_{};
    RHI::RenderPassInfo currentPass_{};
};

} // namespace

GLContext::GLContext(GLApi api)
    : api_(api) {}

std::unique_ptr<RHI::ICommandBuffer> GLContext::AllocateCommandBuffer() {
    return std::make_unique<GLCommandBuffer>(api_);
}

std::unique_ptr<RHI::IPipeline> GLContext::CreatePipeline(const RHI::PipelineDesc& desc) {
    return std::make_unique<GLPipeline>(desc);
}

void GLContext::BindTexture(const RHI::u32 slot, const RHI::Texture& texture) {
    if (api_.ActiveTexture == nullptr || api_.BindTexture == nullptr) {
        return;
    }

    api_.ActiveTexture(api_.Texture0Enum + slot);
    api_.BindTexture(api_.Texture2DEnum, static_cast<unsigned int>(texture.graphic_handle_i));
}

} // namespace Lvs::Engine::RenderingV2::Backends::OpenGL
