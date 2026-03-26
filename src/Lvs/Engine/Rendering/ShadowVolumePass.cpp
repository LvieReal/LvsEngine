#include "Lvs/Engine/Rendering/Renderer.hpp"

#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"

#include <array>

namespace Lvs::Engine::Rendering {

void ShadowVolumePassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    Renderer* renderer,
    const Pipeline* volumePipeline,
    const Pipeline* maskClearPipeline,
    const Pipeline* maskPipeline,
    const RHI::IResourceSet* globalResources
) {
    surface_ = surface;
    scene_ = scene;
    renderer_ = renderer;
    volumePipeline_ = volumePipeline;
    maskClearPipeline_ = maskClearPipeline;
    maskPipeline_ = maskPipeline;
    globalResources_ = globalResources;
}

void ShadowVolumePassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || renderer_ == nullptr || !scene_->EnableShadowVolumes) {
        return;
    }
    if (volumePipeline_ == nullptr || maskClearPipeline_ == nullptr || maskPipeline_ == nullptr || globalResources_ == nullptr) {
        return;
    }
    if (scene_->ShadowCasters.empty() || scene_->GeometryTarget.Framebuffer == nullptr) {
        return;
    }

    const RHI::RenderPassInfo volumePass{
        .width = scene_->GeometryTarget.Width,
        .height = scene_->GeometryTarget.Height,
        .colorAttachmentCount = scene_->GeometryTarget.ColorAttachmentCount,
        .renderPassHandle = scene_->GeometryTarget.RenderPass,
        .framebufferHandle = scene_->GeometryTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = false
    };

    cmd.BeginRenderPass(volumePass);
    cmd.BindPipeline(*volumePipeline_);
    cmd.BindResourceSet(0, *globalResources_);

    for (const auto& draw : scene_->ShadowCasters) {
        const auto* mesh = draw.Mesh;
        if (mesh == nullptr || mesh->VertexBuffer == nullptr || mesh->AdjacencyIndexBuffer == nullptr || mesh->AdjacencyIndexCount == 0) {
            continue;
        }

        cmd.BindVertexBuffer(0, *mesh->VertexBuffer, mesh->VertexOffset);
        cmd.BindIndexBuffer(*mesh->AdjacencyIndexBuffer, mesh->AdjacencyIndexBufferType, 0);

        Common::ShadowVolumePushConstants push{};
        push.Data = {draw.BaseInstance, 0U, 0U, 0U};
        push.LightDirExtrude = scene_->ShadowVolumeLightDirExtrude;
        push.Params = {
            scene_->ShadowVolumeBias,
            static_cast<float>(scene_->ShadowVolumeCapMode),
            0.0F,
            0.0F
        };
        const std::array<RHI::ICommandBuffer::PushConstantField, 3> fields{
            RHI::ICommandBuffer::PushConstantField{
                .name = "pushData.data",
                .type = RHI::ICommandBuffer::PushConstantFieldType::UInt4,
                .data = push.Data.data()
            },
            RHI::ICommandBuffer::PushConstantField{
                .name = "pushData.lightDirExtrude",
                .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                .data = push.LightDirExtrude.data()
            },
            RHI::ICommandBuffer::PushConstantField{
                .name = "pushData.params",
                .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                .data = push.Params.data()
            }
        };
        cmd.PushConstants(RHI::ICommandBuffer::PushConstantsInfo{
            .data = &push,
            .size = sizeof(push),
            .fields = fields.data(),
            .fieldCount = static_cast<RHI::u32>(fields.size())
        });

        cmd.Draw(RHI::ICommandBuffer::DrawInfo{
            .vertexCount = 0,
            .indexCount = mesh->AdjacencyIndexCount,
            .instanceCount = draw.InstanceCount
        });
    }

    cmd.EndRenderPass();

    const RHI::RenderPassInfo maskPass{
        .width = scene_->GeometryTarget.Width,
        .height = scene_->GeometryTarget.Height,
        .colorAttachmentCount = scene_->GeometryTarget.ColorAttachmentCount,
        .renderPassHandle = scene_->GeometryTarget.RenderPass,
        .framebufferHandle = scene_->GeometryTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = false
    };
    cmd.BeginRenderPass(maskPass);
    cmd.BindPipeline(*maskClearPipeline_);
    cmd.BindResourceSet(0, *globalResources_);
    cmd.Draw(RHI::ICommandBuffer::DrawInfo{.vertexCount = 3, .indexCount = 0, .instanceCount = 1});
    cmd.BindPipeline(*maskPipeline_);
    cmd.Draw(RHI::ICommandBuffer::DrawInfo{.vertexCount = 3, .indexCount = 0, .instanceCount = 1});
    cmd.EndRenderPass();
}

} // namespace Lvs::Engine::Rendering
