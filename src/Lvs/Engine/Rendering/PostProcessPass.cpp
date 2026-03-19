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

void PostProcessPassRenderer::SetInputs(
    const RenderSurface* surface,
    const SceneData* scene,
    const Pipeline* compositePipeline,
    const Pipeline* blurDownPipeline,
    const Pipeline* blurUpPipeline
) {
    surface_ = surface;
    scene_ = scene;
    compositePipeline_ = compositePipeline;
    blurDownPipeline_ = blurDownPipeline;
    blurUpPipeline_ = blurUpPipeline;
}

void PostProcessPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || !scene_->EnablePostProcess) {
        return;
    }
    if (compositePipeline_ == nullptr || blurDownPipeline_ == nullptr || blurUpPipeline_ == nullptr) {
        return;
    }
    if (scene_->PostCompositeResources == nullptr) {
        return;
    }

    const RHI::u32 levelsUsed = std::max<RHI::u32>(1U, std::min(scene_->PostBlurLevelCount, SceneData::MaxPostBlurLevels));
    const float blurAmount = std::max(1.0F, scene_->NeonBlur);
    RHI::u32 sourceWidth = std::max(1U, scene_->GeometryTarget.Width);
    RHI::u32 sourceHeight = std::max(1U, scene_->GeometryTarget.Height);

    for (RHI::u32 level = 0; level < levelsUsed; ++level) {
        const auto& target = scene_->PostBlurDownLevelTargets[level];
        const auto* resources = scene_->PostBlurDownLevelResources[level];
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
            const auto& target = scene_->PostBlurUpLevelTargets[static_cast<std::size_t>(level)];
            const auto* resources = scene_->PostBlurUpLevelResources[static_cast<std::size_t>(level)];
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

    if (scene_->PostBlurFinalResources != nullptr && scene_->PostBlurFinalTarget.Framebuffer != nullptr) {
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
            .width = scene_->PostBlurFinalTarget.Width,
            .height = scene_->PostBlurFinalTarget.Height,
            .colorAttachmentCount = scene_->PostBlurFinalTarget.ColorAttachmentCount,
            .renderPassHandle = scene_->PostBlurFinalTarget.RenderPass,
            .framebufferHandle = scene_->PostBlurFinalTarget.Framebuffer,
            .clearColor = true,
            .clearDepth = false
        };
        RecordFullScreenPass(
            cmd,
            finalPass,
            *blurUpPipeline_,
            *scene_->PostBlurFinalResources,
            &push
        );
    }

    const RHI::RenderPassInfo compositePass{
        .width = scene_->PostProcessTarget.Width,
        .height = scene_->PostProcessTarget.Height,
        .colorAttachmentCount = scene_->PostProcessTarget.ColorAttachmentCount,
        .renderPassHandle = scene_->PostProcessTarget.RenderPass,
        .framebufferHandle = scene_->PostProcessTarget.Framebuffer,
        .clearColor = false,
        .clearDepth = false
    };

    const std::array<RHI::ICommandBuffer::PushConstantField, 1> compositeFields{
        RHI::ICommandBuffer::PushConstantField{
            .name = "pushData.settings",
            .type = RHI::ICommandBuffer::PushConstantFieldType::Float4,
            .data = scene_->PostProcessPush.Settings.data()
        }
    };
    const RHI::ICommandBuffer::PushConstantsInfo compositePush{
        .data = &scene_->PostProcessPush,
        .size = sizeof(scene_->PostProcessPush),
        .fields = compositeFields.data(),
        .fieldCount = compositeFields.size()
    };

    RecordFullScreenPass(cmd, compositePass, *compositePipeline_, *scene_->PostCompositeResources, &compositePush);
}

} // namespace Lvs::Engine::Rendering
