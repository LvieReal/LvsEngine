#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <array>
#include <algorithm>

namespace Lvs::Engine::Rendering {

namespace {

void RecordFullScreenPass(
    RHI::ICommandBuffer& cmd,
    const RHI::RenderPassInfo& passInfo,
    const Pipeline& pipeline,
    const RHI::IResourceSet& resources,
    const RHI::ICommandBuffer::PushConstantsInfo* push
) {
    cmd.BeginRenderPass(passInfo);
    cmd.BindPipeline(pipeline);
    cmd.BindResourceSet(0, resources);
    if (push != nullptr && push->fields != nullptr && push->fieldCount > 0) {
        cmd.PushConstants(*push);
    }
    cmd.Draw(RHI::ICommandBuffer::DrawInfo{.vertexCount = 3, .indexCount = 0, .instanceCount = 1});
    cmd.EndRenderPass();
}

} // namespace

void HbaoPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    const Pipeline* hbaoPipeline,
    const Pipeline* blurDownPipeline,
    const Pipeline* blurUpPipeline
) {
    surface_ = surface;
    scene_ = scene;
    hbaoPipeline_ = hbaoPipeline;
    blurDownPipeline_ = blurDownPipeline;
    blurUpPipeline_ = blurUpPipeline;
}

void HbaoPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || !scene_->EnableHbao) {
        return;
    }
    if (hbaoPipeline_ == nullptr || blurDownPipeline_ == nullptr || blurUpPipeline_ == nullptr) {
        return;
    }
    if (scene_->HbaoResources == nullptr || scene_->HbaoTarget.Framebuffer == nullptr) {
        return;
    }

    const std::array<RHI::ICommandBuffer::PushConstantField, 3> hbaoFields{
        RHI::ICommandBuffer::PushConstantField{
            .name = "pushData.params0",
            .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
            .data = scene_->HbaoPush.Params0.data()
        },
        RHI::ICommandBuffer::PushConstantField{
            .name = "pushData.params1",
            .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
            .data = scene_->HbaoPush.Params1.data()
        },
        RHI::ICommandBuffer::PushConstantField{
            .name = "pushData.params2",
            .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
            .data = scene_->HbaoPush.Params2.data()
        }
    };
    const RHI::ICommandBuffer::PushConstantsInfo hbaoPush{
        .data = &scene_->HbaoPush,
        .size = sizeof(scene_->HbaoPush),
        .fields = hbaoFields.data(),
        .fieldCount = hbaoFields.size()
    };

    const RHI::RenderPassInfo hbaoPass{
        .width = scene_->HbaoTarget.Width,
        .height = scene_->HbaoTarget.Height,
        .colorAttachmentCount = scene_->HbaoTarget.ColorAttachmentCount,
        .renderPassHandle = scene_->HbaoTarget.RenderPass,
        .framebufferHandle = scene_->HbaoTarget.Framebuffer,
        .clearColor = true,
        .clearDepth = false
    };
    RecordFullScreenPass(cmd, hbaoPass, *hbaoPipeline_, *scene_->HbaoResources, &hbaoPush);

    const RHI::u32 levelsUsed = std::max<RHI::u32>(1U, std::min(scene_->HbaoBlurLevelCount, SceneData::MaxPostBlurLevels));
    const float blurAmount = std::max(0.0F, scene_->HbaoBlur);
    RHI::u32 sourceWidth = std::max(1U, scene_->HbaoTarget.Width);
    RHI::u32 sourceHeight = std::max(1U, scene_->HbaoTarget.Height);

    for (RHI::u32 level = 0; level < levelsUsed; ++level) {
        const auto& target = scene_->HbaoBlurDownLevelTargets[level];
        const auto* resources = scene_->HbaoBlurDownLevelResources[level];
        if (resources == nullptr || target.Framebuffer == nullptr) {
            continue;
        }
        const Common::PostProcessPushConstants blurSettings{
            .Settings = {1.0F / static_cast<float>(std::max(1U, sourceWidth)),
                         1.0F / static_cast<float>(std::max(1U, sourceHeight)),
                         blurAmount,
                         0.0F}
        };
        const std::array<RHI::ICommandBuffer::PushConstantField, 1> fields{
            RHI::ICommandBuffer::PushConstantField{
                .name = "pushData.settings",
                .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                .data = blurSettings.Settings.data()
            }
        };
        const RHI::ICommandBuffer::PushConstantsInfo push{
            .data = &blurSettings,
            .size = sizeof(blurSettings),
            .fields = fields.data(),
            .fieldCount = fields.size()
        };
        const RHI::RenderPassInfo pass{
            .width = target.Width,
            .height = target.Height,
            .colorAttachmentCount = target.ColorAttachmentCount,
            .renderPassHandle = target.RenderPass,
            .framebufferHandle = target.Framebuffer,
            .clearColor = true,
            .clearDepth = false
        };
        RecordFullScreenPass(cmd, pass, *blurDownPipeline_, *resources, &push);
        sourceWidth = std::max(1U, target.Width);
        sourceHeight = std::max(1U, target.Height);
    }

    if (levelsUsed > 1U) {
        for (int level = static_cast<int>(levelsUsed) - 2; level >= 0; --level) {
            const auto& target = scene_->HbaoBlurUpLevelTargets[static_cast<std::size_t>(level)];
            const auto* resources = scene_->HbaoBlurUpLevelResources[static_cast<std::size_t>(level)];
            if (resources == nullptr || target.Framebuffer == nullptr) {
                continue;
            }
            const Common::PostProcessPushConstants blurSettings{
                .Settings = {1.0F / static_cast<float>(std::max(1U, sourceWidth)),
                             1.0F / static_cast<float>(std::max(1U, sourceHeight)),
                             blurAmount,
                             0.0F}
            };
            const std::array<RHI::ICommandBuffer::PushConstantField, 1> fields{
                RHI::ICommandBuffer::PushConstantField{
                    .name = "pushData.settings",
                    .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                    .data = blurSettings.Settings.data()
                }
            };
            const RHI::ICommandBuffer::PushConstantsInfo push{
                .data = &blurSettings,
                .size = sizeof(blurSettings),
                .fields = fields.data(),
                .fieldCount = fields.size()
            };
            const RHI::RenderPassInfo pass{
                .width = target.Width,
                .height = target.Height,
                .colorAttachmentCount = target.ColorAttachmentCount,
                .renderPassHandle = target.RenderPass,
                .framebufferHandle = target.Framebuffer,
                .clearColor = true,
                .clearDepth = false
            };
            RecordFullScreenPass(cmd, pass, *blurUpPipeline_, *resources, &push);
            sourceWidth = std::max(1U, target.Width);
            sourceHeight = std::max(1U, target.Height);
        }
    }

    if (scene_->HbaoBlurFinalResources != nullptr && scene_->HbaoBlurFinalTarget.Framebuffer != nullptr) {
        const Common::PostProcessPushConstants blurSettings{
            .Settings = {1.0F / static_cast<float>(std::max(1U, sourceWidth)),
                         1.0F / static_cast<float>(std::max(1U, sourceHeight)),
                         blurAmount,
                         0.0F}
        };
        const std::array<RHI::ICommandBuffer::PushConstantField, 1> fields{
            RHI::ICommandBuffer::PushConstantField{
                .name = "pushData.settings",
                .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
                .data = blurSettings.Settings.data()
            }
        };
        const RHI::ICommandBuffer::PushConstantsInfo push{
            .data = &blurSettings,
            .size = sizeof(blurSettings),
            .fields = fields.data(),
            .fieldCount = fields.size()
        };
        const RHI::RenderPassInfo finalPass{
            .width = scene_->HbaoBlurFinalTarget.Width,
            .height = scene_->HbaoBlurFinalTarget.Height,
            .colorAttachmentCount = scene_->HbaoBlurFinalTarget.ColorAttachmentCount,
            .renderPassHandle = scene_->HbaoBlurFinalTarget.RenderPass,
            .framebufferHandle = scene_->HbaoBlurFinalTarget.Framebuffer,
            .clearColor = true,
            .clearDepth = false
        };
        RecordFullScreenPass(cmd, finalPass, *blurUpPipeline_, *scene_->HbaoBlurFinalResources, &push);
    }
}

} // namespace Lvs::Engine::Rendering
