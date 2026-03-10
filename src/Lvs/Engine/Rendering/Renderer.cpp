#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering {

void Renderer::Initialize(RHI::IContext& ctx, const RenderSurface& surface) {
    static_cast<void>(ctx);
    surface_ = surface;
}

void Renderer::RecordFrameCommands(
    RHI::IContext& ctx,
    RHI::ICommandBuffer& cmd,
    const SceneData& scene,
    const RHI::u32 frameIndex
) {
    static_cast<void>(frameIndex);
    sceneRenderPassHandle_ = scene.GeometryTarget.RenderPass;
    sceneColorAttachmentCount_ = std::max(1U, scene.GeometryTarget.ColorAttachmentCount);

    if (scene.ClearColor) {
        const RHI::RenderPassInfo clearPass{
            .width = surface_.Width,
            .height = surface_.Height,
            .colorAttachmentCount = scene.GeometryTarget.ColorAttachmentCount,
            .renderPassHandle = scene.GeometryTarget.RenderPass,
            .framebufferHandle = scene.GeometryTarget.Framebuffer,
            .clearColor = true,
            .clearColorValue = {
                scene.ClearColorValue[0],
                scene.ClearColorValue[1],
                scene.ClearColorValue[2],
                scene.ClearColorValue[3]
            },
            .clearDepth = true,
            .clearDepthValue = 0.0F
        };
        cmd.BeginRenderPass(clearPass);
        cmd.EndRenderPass();
    }

    Pipeline* skyboxPipeline = scene.EnableSkybox
                                   ? GetOrCreatePipeline(
                                         ctx,
                                         PassKey::Skybox,
                                         RHI::CullMode::Front,
                                         scene.SkyboxTarget.RenderPass,
                                         scene.SkyboxTarget.ColorAttachmentCount
                                     )
                                   : nullptr;
    Pipeline* postProcessPipeline = scene.EnablePostProcess
                                        ? GetOrCreatePipeline(
                                              ctx,
                                              PassKey::PostProcess,
                                              RHI::CullMode::None,
                                              scene.PostProcessTarget.RenderPass,
                                              scene.PostProcessTarget.ColorAttachmentCount
                                          )
                                        : nullptr;
    Pipeline* blurDownPipeline = scene.EnablePostProcess
                                     ? GetOrCreatePipeline(
                                           ctx,
                                           PassKey::PostBlurDown,
                                           RHI::CullMode::None,
                                           scene.PostBlurDownTarget.RenderPass,
                                           scene.PostBlurDownTarget.ColorAttachmentCount
                                       )
                                     : nullptr;
    Pipeline* blurUpPipeline = scene.EnablePostProcess
                                   ? GetOrCreatePipeline(
                                         ctx,
                                         PassKey::PostBlurUp,
                                         RHI::CullMode::None,
                                         scene.PostBlurUpTarget.RenderPass,
                                         scene.PostBlurUpTarget.ColorAttachmentCount
                                     )
                                   : nullptr;
    Pipeline* geometryPipeline = scene.EnableGeometry
                                     ? GetOrCreatePipeline(
                                           ctx,
                                           PassKey::Geometry,
                                           RHI::CullMode::Back,
                                           scene.GeometryTarget.RenderPass,
                                           scene.GeometryTarget.ColorAttachmentCount
                                       )
                                     : nullptr;
    const RHI::IResourceSet* globalResources = GetOrCreateGlobalResources(ctx, scene);

    geometryPass_.SetInputs(&surface_, &scene, this, geometryPipeline, globalResources);
    shadowPass_.SetInputs(&surface_, &scene, this, scene.ShadowResources);
    skyboxPass_.SetInputs(&surface_, &scene, skyboxPipeline, globalResources);
    postProcessPass_.SetInputs(
        &surface_,
        &scene,
        postProcessPipeline,
        blurDownPipeline,
        blurUpPipeline
    );

    shadowPass_.RecordCommands(ctx, cmd);
    skyboxPass_.RecordCommands(ctx, cmd);
    geometryPass_.RecordCommands(ctx, cmd);
    postProcessPass_.RecordCommands(ctx, cmd);
}

Pipeline* Renderer::GetOrCreatePipeline(
    RHI::IContext& ctx,
    const PassKey key,
    const RHI::CullMode cullMode,
    void* renderPassHandle,
    const RHI::u32 colorAttachmentCount
) {
    const std::size_t renderPassBits = reinterpret_cast<std::size_t>(renderPassHandle) >> 3U;
    const std::size_t cacheKey = (static_cast<std::size_t>(key) << 8U) | static_cast<std::size_t>(cullMode) |
                                 (renderPassBits << 16U) | (static_cast<std::size_t>(colorAttachmentCount) << 4U);
    const auto it = pipelineCache_.find(cacheKey);
    if (it != pipelineCache_.end()) {
        return it->second.get();
    }

    RHI::PipelineDesc desc{};
    desc.renderPassHandle = renderPassHandle;
    desc.colorAttachmentCount = colorAttachmentCount;
    switch (key) {
        case PassKey::Geometry:
            desc.pipelineId = "main";
            desc.vertexLayout = RHI::VertexLayout::P3N3;
            desc.depthTest = true;
            desc.depthWrite = true;
            desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        case PassKey::Shadow:
            desc.pipelineId = "shadow";
            desc.vertexLayout = RHI::VertexLayout::P3N3;
            desc.depthTest = true;
            desc.depthWrite = true;
            desc.depthCompare = RHI::DepthCompare::LessOrEqual;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        case PassKey::Skybox:
            desc.pipelineId = "sky";
            desc.vertexLayout = RHI::VertexLayout::P3;
            desc.depthTest = true;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        case PassKey::PostProcess:
            desc.pipelineId = "post_composite";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = true;
            desc.cullMode = cullMode;
            break;
        case PassKey::PostBlurDown:
            desc.pipelineId = "post_blur_down";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        case PassKey::PostBlurUp:
            desc.pipelineId = "post_blur_up";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        default:
            break;
    }

    auto pipeline = ctx.CreatePipeline(desc);
    Pipeline* ptr = pipeline.get();
    pipelineCache_.emplace(cacheKey, std::move(pipeline));
    return ptr;
}

Pipeline* Renderer::GetOrCreateGeometryPipeline(
    RHI::IContext& ctx,
    const RHI::CullMode cullMode,
    const bool transparent,
    const bool alwaysOnTop
) {
    const std::size_t modeBits = (transparent ? 1ULL : 0ULL) | (alwaysOnTop ? 2ULL : 0ULL);
    const std::size_t renderPassBits = reinterpret_cast<std::size_t>(sceneRenderPassHandle_) >> 3U;
    const std::size_t cacheKey = (static_cast<std::size_t>(PassKey::Geometry) << 8U) | static_cast<std::size_t>(cullMode) |
                                 (modeBits << 16U) | (renderPassBits << 24U) |
                                 (static_cast<std::size_t>(sceneColorAttachmentCount_) << 4U);
    const auto it = pipelineCache_.find(cacheKey);
    if (it != pipelineCache_.end()) {
        return it->second.get();
    }

    RHI::PipelineDesc desc{};
    desc.pipelineId = "main";
    desc.vertexLayout = RHI::VertexLayout::P3N3;
    desc.renderPassHandle = sceneRenderPassHandle_;
    desc.colorAttachmentCount = sceneColorAttachmentCount_;
    desc.cullMode = cullMode;

    if (alwaysOnTop) {
        desc.depthTest = false;
        desc.depthWrite = false;
        desc.depthCompare = RHI::DepthCompare::Always;
        desc.blending = true;
    } else if (transparent) {
        desc.depthTest = true;
        desc.depthWrite = false;
        desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
        desc.blending = true;
    } else {
        desc.depthTest = true;
        desc.depthWrite = true;
        desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
        desc.blending = false;
    }

    auto pipeline = ctx.CreatePipeline(desc);
    Pipeline* ptr = pipeline.get();
    pipelineCache_.emplace(cacheKey, std::move(pipeline));
    return ptr;
}

const RHI::IResourceSet* Renderer::GetOrCreateGlobalResources(RHI::IContext& ctx, const SceneData& scene) {
    if (scene.GlobalResources != nullptr) {
        return scene.GlobalResources;
    }
    if (scene.GlobalBindings == nullptr || scene.GlobalBindingCount == 0) {
        return nullptr;
    }

    const auto it = resourceSetCache_.find(scene.GlobalResourceKey);
    if (it != resourceSetCache_.end()) {
        return it->second.get();
    }

    const RHI::ResourceSetDesc desc{
        .bindings = scene.GlobalBindings,
        .bindingCount = scene.GlobalBindingCount
    };
    auto set = ctx.CreateResourceSet(desc);
    const RHI::IResourceSet* ptr = set.get();
    resourceSetCache_.emplace(scene.GlobalResourceKey, std::move(set));
    return ptr;
}

} // namespace Lvs::Engine::Rendering
