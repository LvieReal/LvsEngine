#include "Lvs/Engine/Rendering/Renderer.hpp"

namespace Lvs::Engine::Rendering {

void SkyboxPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    const Pipeline* pipeline,
    const RHI::IResourceSet* resources
) {
    surface_ = surface;
    scene_ = scene;
    pipeline_ = pipeline;
    resources_ = resources;
}

void SkyboxPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || pipeline_ == nullptr || resources_ == nullptr || !scene_->EnableSkybox) {
        return;
    }
    const auto* mesh = scene_->SkyboxDraw.Mesh;
    if (mesh == nullptr || mesh->VertexBuffer == nullptr || mesh->IndexBuffer == nullptr || mesh->IndexCount == 0) {
        return;
    }

    const RHI::RenderPassInfo renderPass{
        .width = surface_->Width,
        .height = surface_->Height,
        .renderPassHandle = scene_->SkyboxTarget.RenderPass,
        .framebufferHandle = scene_->SkyboxTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = false
    };

    cmd.BeginRenderPass(renderPass);
    cmd.BindPipeline(*pipeline_);
    cmd.BindVertexBuffer(0, *mesh->VertexBuffer, mesh->VertexOffset);
    cmd.BindIndexBuffer(*mesh->IndexBuffer, mesh->IndexBufferType, mesh->IndexOffset);
    cmd.BindResourceSet(0, *resources_);
    cmd.PushConstants(&scene_->SkyboxPush, sizeof(scene_->SkyboxPush));
    cmd.DrawIndexed(mesh->IndexCount);
    cmd.EndRenderPass();
}

} // namespace Lvs::Engine::Rendering
