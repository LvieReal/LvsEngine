#pragma once

#include "Lvs/Engine/RenderingV2/Backends/OpenGL/GLApi.hpp"
#include "Lvs/Engine/RenderingV2/RHI/IContext.hpp"

namespace Lvs::Engine::RenderingV2::Backends::OpenGL {

class GLContext final : public RHI::IContext {
public:
    explicit GLContext(GLApi api);
    std::unique_ptr<RHI::ICommandBuffer> AllocateCommandBuffer() override;
    std::unique_ptr<RHI::IPipeline> CreatePipeline(const RHI::PipelineDesc& desc) override;
    void BindTexture(RHI::u32 slot, const RHI::Texture& texture) override;

private:
    GLApi api_;
};

} // namespace Lvs::Engine::RenderingV2::Backends::OpenGL
