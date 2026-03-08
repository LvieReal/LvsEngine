#include "Lvs/Engine/RenderingV2/Renderer.hpp"

namespace Lvs::Engine::RenderingV2 {

namespace {

RHI::RenderPassInfo MakeDefaultPassInfo(const RenderSurface& surface, const bool clearColor, const bool clearDepth) {
    RHI::RenderPassInfo info{};
    info.width = surface.Width;
    info.height = surface.Height;
    info.clearColor = clearColor;
    info.clearDepth = clearDepth;
    return info;
}

} // namespace

void ShadowPassRenderer::SetInputs(const RenderSurface* surface, const SceneData* scene, const Pipeline* pipeline) {
    surface_ = surface;
    scene_ = scene;
    pipeline_ = pipeline;
}

void ShadowPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || pipeline_ == nullptr || !scene_->EnableShadows || scene_->ShadowIndexCount == 0) {
        return;
    }

    const RHI::RenderPassInfo passInfo = MakeDefaultPassInfo(*surface_, false, true);
    cmd.BeginRenderPass(passInfo);
    cmd.BindPipeline(*pipeline_);
    cmd.BindResourceSet(0, scene_->GlobalResources);
    cmd.DrawIndexed(scene_->ShadowIndexCount);
    cmd.EndRenderPass();
}

void SkyboxPassRenderer::SetInputs(const RenderSurface* surface, const SceneData* scene, const Pipeline* pipeline) {
    surface_ = surface;
    scene_ = scene;
    pipeline_ = pipeline;
}

void SkyboxPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || pipeline_ == nullptr || !scene_->EnableSkybox || scene_->SkyboxIndexCount == 0) {
        return;
    }

    const RHI::RenderPassInfo passInfo = MakeDefaultPassInfo(*surface_, false, false);
    cmd.BeginRenderPass(passInfo);
    cmd.BindPipeline(*pipeline_);
    cmd.BindResourceSet(0, scene_->GlobalResources);
    cmd.DrawIndexed(scene_->SkyboxIndexCount);
    cmd.EndRenderPass();
}

void PostProcessPassRenderer::SetInputs(const RenderSurface* surface, const SceneData* scene, const Pipeline* pipeline) {
    surface_ = surface;
    scene_ = scene;
    pipeline_ = pipeline;
}

void PostProcessPassRenderer::RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd) {
    static_cast<void>(ctx);
    if (surface_ == nullptr || scene_ == nullptr || pipeline_ == nullptr || !scene_->EnablePostProcess) {
        return;
    }

    const RHI::RenderPassInfo passInfo = MakeDefaultPassInfo(*surface_, false, false);
    cmd.BeginRenderPass(passInfo);
    cmd.BindPipeline(*pipeline_);
    cmd.BindResourceSet(0, scene_->GlobalResources);
    cmd.DrawIndexed(scene_->OpaqueIndexCount + scene_->TransparentIndexCount);
    cmd.EndRenderPass();
}

void Renderer::Initialize(RHI::IContext& ctx, const RenderSurface& surface) {
    surface_ = surface;
    static_cast<void>(GetOrCreatePipeline(ctx, PassKey::Shadow));
    static_cast<void>(GetOrCreatePipeline(ctx, PassKey::Skybox));
    static_cast<void>(GetOrCreatePipeline(ctx, PassKey::PostProcess));
}

void Renderer::RecordFrameCommands(
    RHI::IContext& ctx,
    RHI::ICommandBuffer& cmd,
    const SceneData& scene,
    const RHI::u32 frameIndex
) {
    static_cast<void>(frameIndex);

    Pipeline* shadowPipeline = GetOrCreatePipeline(ctx, PassKey::Shadow);
    Pipeline* skyboxPipeline = GetOrCreatePipeline(ctx, PassKey::Skybox);
    Pipeline* postProcessPipeline = GetOrCreatePipeline(ctx, PassKey::PostProcess);

    shadowPass_.SetInputs(&surface_, &scene, shadowPipeline);
    skyboxPass_.SetInputs(&surface_, &scene, skyboxPipeline);
    postProcessPass_.SetInputs(&surface_, &scene, postProcessPipeline);

    shadowPass_.RecordCommands(ctx, cmd);
    skyboxPass_.RecordCommands(ctx, cmd);
    postProcessPass_.RecordCommands(ctx, cmd);
}

Pipeline* Renderer::GetOrCreatePipeline(RHI::IContext& ctx, const PassKey key) {
    const std::size_t cacheKey = static_cast<std::size_t>(key);
    const auto it = pipelineCache_.find(cacheKey);
    if (it != pipelineCache_.end()) {
        return it->second.get();
    }

    RHI::PipelineDesc desc{};
    switch (key) {
        case PassKey::Shadow:
            desc.depthTest = true;
            desc.depthWrite = true;
            desc.blending = false;
            break;
        case PassKey::Skybox:
            desc.depthTest = true;
            desc.depthWrite = false;
            desc.blending = false;
            break;
        case PassKey::PostProcess:
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.blending = true;
            break;
        default:
            break;
    }

    auto pipeline = ctx.CreatePipeline(desc);
    Pipeline* ptr = pipeline.get();
    pipelineCache_.emplace(cacheKey, std::move(pipeline));
    return ptr;
}

} // namespace Lvs::Engine::RenderingV2
