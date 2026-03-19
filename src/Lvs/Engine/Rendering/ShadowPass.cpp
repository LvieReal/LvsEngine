#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <array>

namespace Lvs::Engine::Rendering {

void ShadowPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    Renderer* renderer
) {
    surface_ = surface;
    scene_ = scene;
    renderer_ = renderer;
}

void ShadowPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    if (surface_ == nullptr || scene_ == nullptr || renderer_ == nullptr || !scene_->EnableShadows) {
        return;
    }
    if (scene_->DirectionalShadowCount == 0 || scene_->ShadowCasters.empty()) {
        return;
    }

    const RHI::u32 shadowCount = std::min(scene_->DirectionalShadowCount, SceneData::MaxDirectionalShadowMaps);
    for (RHI::u32 shadowIndex = 0; shadowIndex < shadowCount; ++shadowIndex) {
        const RHI::IResourceSet* resources = scene_->DirectionalShadowResources[shadowIndex];
        if (resources == nullptr) {
            continue;
        }
        const RHI::u32 cascadeCount = std::min(scene_->DirectionalShadowCascadeCounts[shadowIndex], SceneData::MaxShadowCascades);
        for (RHI::u32 cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex) {
            const auto& target = scene_->DirectionalShadowCascadeTargets[shadowIndex][cascadeIndex];
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
            cmd.BindResourceSet(0, *resources);

            const Pipeline* boundPipeline = nullptr;
            RHI::CullMode boundCullMode = RHI::CullMode::Back;
            for (const auto& draw : scene_->ShadowCasters) {
                const auto* mesh = draw.Mesh;
                if (mesh == nullptr || mesh->VertexBuffer == nullptr || mesh->IndexBuffer == nullptr || mesh->IndexCount == 0) {
                    continue;
                }
                const RHI::CullMode shadowCullMode =
                    (draw.CullMode == RHI::CullMode::None) ? RHI::CullMode::None : RHI::CullMode::Back;
                if (boundPipeline == nullptr || shadowCullMode != boundCullMode) {
                    Pipeline* pipeline = renderer_->GetOrCreatePipeline(
                        ctx,
                        Renderer::PassKey::Shadow,
                        shadowCullMode,
                        target.RenderPass,
                        target.ColorAttachmentCount,
                        target.SampleCount
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
                const Common::ShadowDrawCallPushConstants push{.Data = {draw.BaseInstance, cascadeIndex, 0U, 0U}};
                const std::array<RHI::ICommandBuffer::PushConstantField, 1> fields{
                    RHI::ICommandBuffer::PushConstantField{
                        .name = "pushData.data",
                        .type = RHI::ICommandBuffer::PushConstantFieldType::UInt4,
                        .data = push.Data.data()
                    }
                };
                cmd.PushConstants(RHI::ICommandBuffer::PushConstantsInfo{
                    .data = &push,
                    .size = sizeof(push),
                    .fields = fields.data(),
                    .fieldCount = fields.size()
                });
                cmd.Draw(RHI::ICommandBuffer::DrawInfo{
                    .vertexCount = 0,
                    .indexCount = mesh->IndexCount,
                    .instanceCount = draw.InstanceCount
                });
            }
            cmd.EndRenderPass();
        }
    }
}

} // namespace Lvs::Engine::Rendering
