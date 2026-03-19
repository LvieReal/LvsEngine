#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <array>

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
        .width = scene_->SkyboxTarget.Width,
        .height = scene_->SkyboxTarget.Height,
        .colorAttachmentCount = 1,
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
    const std::array<RHI::ICommandBuffer::PushConstantField, 2> fields{
        RHI::ICommandBuffer::PushConstantField{
            .name = "skyPush.viewProjection",
            .type = RHI::ICommandBuffer::PushConstantFieldType::Matrix4x4,
            .data = scene_->SkyboxPush.ViewProjection.data()
        },
        RHI::ICommandBuffer::PushConstantField{
            .name = "skyPush.tint",
            .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
            .data = scene_->SkyboxPush.Tint.data()
        }
    };
    cmd.PushConstants(RHI::ICommandBuffer::PushConstantsInfo{
        .data = &scene_->SkyboxPush,
        .size = sizeof(scene_->SkyboxPush),
        .fields = fields.data(),
        .fieldCount = fields.size()
    });
    cmd.Draw(RHI::ICommandBuffer::DrawInfo{.vertexCount = 0, .indexCount = mesh->IndexCount, .instanceCount = 1});
    cmd.EndRenderPass();
}

} // namespace Lvs::Engine::Rendering
