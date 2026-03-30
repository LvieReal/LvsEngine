#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <algorithm>
#include <array>

namespace Lvs::Engine::Rendering {

void Image3DPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    Renderer* renderer,
    const RHI::IResourceSet* globalResources
) {
    surface_ = surface;
    scene_ = scene;
    renderer_ = renderer;
    globalResources_ = globalResources;
}

void Image3DPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    if (surface_ == nullptr || scene_ == nullptr || renderer_ == nullptr || globalResources_ == nullptr) {
        return;
    }
    if (scene_->Image3DDraws.empty()) {
        return;
    }

    const RHI::RenderPassInfo renderPass{
        .width = scene_->GeometryTarget.Width,
        .height = scene_->GeometryTarget.Height,
        .colorAttachmentCount = scene_->GeometryTarget.ColorAttachmentCount,
        .renderPassHandle = scene_->GeometryTarget.RenderPass,
        .framebufferHandle = scene_->GeometryTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = false
    };

    cmd.BeginRenderPass(renderPass);
    cmd.BindResourceSet(0, *globalResources_);

    std::vector<const SceneData::Image3DDrawPacket*> draws;
    draws.reserve(scene_->Image3DDraws.size());
    for (const auto& draw : scene_->Image3DDraws) {
        draws.push_back(&draw);
    }
    std::sort(draws.begin(), draws.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->SortDepth > rhs->SortDepth;
    });

    for (const auto* draw : draws) {
        if (draw == nullptr || draw->Mesh == nullptr || draw->TextureResources == nullptr) {
            continue;
        }
        if (phase_ == Phase::DepthTestedOnly && draw->AlwaysOnTop) {
            continue;
        }
        if (phase_ == Phase::AlwaysOnTopOnly && !draw->AlwaysOnTop) {
            continue;
        }
        const auto* mesh = draw->Mesh;
        if (mesh->VertexBuffer == nullptr || mesh->IndexBuffer == nullptr || mesh->IndexCount == 0) {
            continue;
        }

        const auto pushDraw = [&](const Common::Image3DPushConstants& push) {
            const std::array<RHI::ICommandBuffer::PushConstantField, 5> fields{
                RHI::ICommandBuffer::PushConstantField{
                    .name = "pushData.model",
                    .type = RHI::ICommandBuffer::PushConstantFieldType::Matrix4x4,
                    .data = push.Model.data()
                },
                RHI::ICommandBuffer::PushConstantField{
                    .name = "pushData.color",
                    .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                    .data = push.Color.data()
                },
                RHI::ICommandBuffer::PushConstantField{
                    .name = "pushData.options",
                    .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                    .data = push.Options.data()
                },
                RHI::ICommandBuffer::PushConstantField{
                    .name = "pushData.outlineColor",
                    .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                    .data = push.OutlineColor.data()
                },
                RHI::ICommandBuffer::PushConstantField{
                    .name = "pushData.outlineParams",
                    .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                    .data = push.OutlineParams.data()
                }
            };
            cmd.PushConstants(RHI::ICommandBuffer::PushConstantsInfo{
                .data = &push,
                .size = sizeof(push),
                .fields = fields.data(),
                .fieldCount = fields.size()
            });
            cmd.Draw(RHI::ICommandBuffer::DrawInfo{.vertexCount = 0, .indexCount = mesh->IndexCount, .instanceCount = 1U});
        };

        cmd.BindVertexBuffer(0, *mesh->VertexBuffer, mesh->VertexOffset);
        cmd.BindIndexBuffer(*mesh->IndexBuffer, mesh->IndexBufferType, mesh->IndexOffset);
        cmd.BindResourceSet(1, *draw->TextureResources);

        Pipeline* pipeline = renderer_->GetOrCreateImage3DPipeline(ctx, draw->AlwaysOnTop, draw->NegateMask);
        if (pipeline == nullptr) {
            continue;
        }
        cmd.BindPipeline(*pipeline);
        auto push = draw->Push;
        pushDraw(push);
    }

    cmd.EndRenderPass();
}

} // namespace Lvs::Engine::Rendering
