#include "Lvs/Engine/Rendering/Renderer.hpp"

namespace Lvs::Engine::Rendering {

void GeometryPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    Renderer* renderer,
    const Pipeline* pipeline,
    const RHI::IResourceSet* resources
) {
    surface_ = surface;
    scene_ = scene;
    renderer_ = renderer;
    pipeline_ = pipeline;
    resources_ = resources;
}

void GeometryPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    if (surface_ == nullptr || scene_ == nullptr || renderer_ == nullptr || pipeline_ == nullptr || !scene_->EnableGeometry ||
        resources_ == nullptr) {
        return;
    }

    const RHI::RenderPassInfo renderPass{
        .width = surface_->Width,
        .height = surface_->Height,
        .renderPassHandle = scene_->GeometryTarget.RenderPass,
        .framebufferHandle = scene_->GeometryTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = true,
        .clearDepthValue = 0.0F
    };

    cmd.BeginRenderPass(renderPass);
    cmd.BindPipeline(*pipeline_);
    cmd.BindResourceSet(0, *resources_);

    const auto recordDraw = [this, &ctx, &cmd](const SceneData::DrawPacket& draw) {
        const auto* mesh = draw.Mesh;
        if (mesh == nullptr || mesh->VertexBuffer == nullptr || mesh->IndexBuffer == nullptr || mesh->IndexCount == 0) {
            return;
        }
        if (const auto* drawPipeline = renderer_->GetOrCreatePipeline(ctx, Renderer::PassKey::Geometry, draw.CullMode);
            drawPipeline != nullptr) {
            cmd.BindPipeline(*drawPipeline);
        } else {
            return;
        }
        cmd.BindVertexBuffer(0, *mesh->VertexBuffer, mesh->VertexOffset);
        cmd.BindIndexBuffer(*mesh->IndexBuffer, mesh->IndexBufferType, mesh->IndexOffset);
        cmd.PushConstants(&draw.PushConstants, sizeof(draw.PushConstants));
        cmd.DrawIndexed(mesh->IndexCount);
    };

    if (!scene_->GeometryDraws.empty()) {
        for (const auto& draw : scene_->GeometryDraws) {
            recordDraw(draw);
        }
    } else {
        recordDraw(scene_->GeometryDraw);
    }
    cmd.EndRenderPass();
}

} // namespace Lvs::Engine::Rendering
