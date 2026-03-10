#include "Lvs/Engine/Rendering/Renderer.hpp"

namespace Lvs::Engine::Rendering {

void ShadowPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    Renderer* renderer,
    const RHI::IResourceSet* resources
) {
    surface_ = surface;
    scene_ = scene;
    renderer_ = renderer;
    resources_ = resources;
}

void ShadowPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    if (surface_ == nullptr || scene_ == nullptr || renderer_ == nullptr || !scene_->EnableShadows) {
        return;
    }
    if (resources_ == nullptr || scene_->ShadowCascadeCount == 0 || scene_->ShadowCasters.empty()) {
        return;
    }

    const RHI::u32 cascadeCount = std::min(scene_->ShadowCascadeCount, SceneData::MaxShadowCascades);
    for (RHI::u32 cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex) {
        const auto& target = scene_->ShadowCascadeTargets[cascadeIndex];
        if (target.Framebuffer == nullptr) {
            continue;
        }
        const RHI::RenderPassInfo renderPass{
            .width = target.Width,
            .height = target.Height,
            .colorAttachmentCount = target.ColorAttachmentCount,
            .renderPassHandle = target.RenderPass,
            .framebufferHandle = target.Framebuffer,
            .clearColor = false,
            .clearDepth = true,
            .clearDepthValue = 1.0F
        };
        cmd.BeginRenderPass(renderPass);
        cmd.BindResourceSet(0, *resources_);

        const Pipeline* boundPipeline = nullptr;
        RHI::CullMode boundCullMode = RHI::CullMode::Back;
        for (const auto& draw : scene_->ShadowCasters) {
            const auto* mesh = draw.Mesh;
            if (mesh == nullptr || mesh->VertexBuffer == nullptr || mesh->IndexBuffer == nullptr || mesh->IndexCount == 0) {
                continue;
            }
            const RHI::CullMode shadowCullMode = (draw.CullMode == RHI::CullMode::None) ? RHI::CullMode::None : RHI::CullMode::Back;
            if (boundPipeline == nullptr || shadowCullMode != boundCullMode) {
                Pipeline* pipeline = renderer_->GetOrCreatePipeline(
                    ctx,
                    Renderer::PassKey::Shadow,
                    shadowCullMode,
                    target.RenderPass,
                    target.ColorAttachmentCount
                );
                if (pipeline == nullptr) {
                    continue;
                }
                cmd.BindPipeline(*pipeline);
                boundPipeline = pipeline;
                boundCullMode = shadowCullMode;
            }
            cmd.BindVertexBuffer(0, *mesh->VertexBuffer, mesh->VertexOffset);
            cmd.BindIndexBuffer(*mesh->IndexBuffer, mesh->IndexBufferType, mesh->IndexOffset);
            Common::ShadowPushConstants push{};
            push.Model = draw.PushConstants.Model;
            push.Cascade = {static_cast<float>(cascadeIndex), 0.0F, 0.0F, 0.0F};
            cmd.PushConstants(&push, sizeof(push));
            cmd.DrawIndexed(mesh->IndexCount);
        }
        cmd.EndRenderPass();
    }
}

} // namespace Lvs::Engine::Rendering
