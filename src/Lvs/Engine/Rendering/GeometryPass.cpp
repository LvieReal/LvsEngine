#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <algorithm>
#include <vector>

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
        .width = scene_->GeometryTarget.Width,
        .height = scene_->GeometryTarget.Height,
        .colorAttachmentCount = scene_->GeometryTarget.ColorAttachmentCount,
        .renderPassHandle = scene_->GeometryTarget.RenderPass,
        .framebufferHandle = scene_->GeometryTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = false
    };

    cmd.BeginRenderPass(renderPass);
    cmd.BindResourceSet(0, *resources_);

    const auto recordDraw = [this, &ctx, &cmd](const SceneData::DrawPacket& draw) {
        const auto* mesh = draw.Mesh;
        if (mesh == nullptr || mesh->VertexBuffer == nullptr || mesh->IndexBuffer == nullptr || mesh->IndexCount == 0) {
            return;
        }
        if (const auto* drawPipeline =
                renderer_->GetOrCreateGeometryPipeline(ctx, draw.CullMode, draw.Transparent, draw.AlwaysOnTop);
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

    std::vector<const SceneData::DrawPacket*> opaqueDraws{};
    std::vector<const SceneData::DrawPacket*> transparentDraws{};
    std::vector<const SceneData::DrawPacket*> alwaysOnTopDraws{};

    const auto collectDraw = [&opaqueDraws, &transparentDraws, &alwaysOnTopDraws](const SceneData::DrawPacket& draw) {
        if (draw.Mesh == nullptr) {
            return;
        }
        if (draw.AlwaysOnTop) {
            alwaysOnTopDraws.push_back(&draw);
            return;
        }
        if (draw.Transparent) {
            transparentDraws.push_back(&draw);
            return;
        }
        opaqueDraws.push_back(&draw);
    };

    if (!scene_->GeometryDraws.empty()) {
        for (const auto& draw : scene_->GeometryDraws) {
            collectDraw(draw);
        }
    } else {
        collectDraw(scene_->GeometryDraw);
    }

    std::sort(transparentDraws.begin(), transparentDraws.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->SortDepth > rhs->SortDepth;
    });

    for (const auto* draw : opaqueDraws) {
        recordDraw(*draw);
    }
    for (const auto* draw : transparentDraws) {
        recordDraw(*draw);
    }
    for (const auto* draw : alwaysOnTopDraws) {
        recordDraw(*draw);
    }
    cmd.EndRenderPass();
}

} // namespace Lvs::Engine::Rendering
