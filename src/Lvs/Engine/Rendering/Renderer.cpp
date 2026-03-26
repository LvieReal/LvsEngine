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
    sceneSampleCount_ = std::max(1U, scene.GeometryTarget.SampleCount);
    shadowVolumeDepthCompare_ = scene.ShadowVolumeDepthCompare;
    shadowVolumeCullMode_ = scene.ShadowVolumeCullMode;
    shadowVolumeMaskDepthCompare_ = scene.ShadowVolumeMaskDepthCompare;
    shadowVolumeMaskCullMode_ = scene.ShadowVolumeMaskCullMode;
    shadowVolumeStencilMode_ = scene.ShadowVolumeStencilMode;
    shadowVolumeSwapStencilOps_ = scene.ShadowVolumeSwapStencilOps;

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
            .clearDepthValue = 0.0F,
            .clearStencil = scene.EnableShadowVolumes,
            .clearStencilValue = 0U
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
                                         scene.SkyboxTarget.ColorAttachmentCount,
                                         scene.SkyboxTarget.SampleCount
                                     )
                                   : nullptr;
    Pipeline* postProcessPipeline = scene.EnablePostProcess
                                        ? GetOrCreatePipeline(
                                              ctx,
                                              PassKey::PostProcess,
                                              RHI::CullMode::None,
                                              scene.PostProcessTarget.RenderPass,
                                              scene.PostProcessTarget.ColorAttachmentCount,
                                              scene.PostProcessTarget.SampleCount
                                          )
                                        : nullptr;
    Pipeline* blurDownPipeline = scene.EnablePostProcess
                                     ? GetOrCreatePipeline(
                                           ctx,
                                           PassKey::PostBlurDown,
                                           RHI::CullMode::None,
                                           scene.PostBlurDownTarget.RenderPass,
                                           scene.PostBlurDownTarget.ColorAttachmentCount,
                                           scene.PostBlurDownTarget.SampleCount
                                       )
                                     : nullptr;
    Pipeline* blurUpPipeline = scene.EnablePostProcess
                                   ? GetOrCreatePipeline(
                                         ctx,
                                         PassKey::PostBlurUp,
                                         RHI::CullMode::None,
                                         scene.PostBlurUpTarget.RenderPass,
                                         scene.PostBlurUpTarget.ColorAttachmentCount,
                                         scene.PostBlurUpTarget.SampleCount
                                     )
                                   : nullptr;
    Pipeline* hbaoPipeline = scene.EnableHbao
                                 ? GetOrCreatePipeline(
                                       ctx,
                                       PassKey::Hbao,
                                       RHI::CullMode::None,
                                       scene.HbaoTarget.RenderPass,
                                       scene.HbaoTarget.ColorAttachmentCount,
                                       scene.HbaoTarget.SampleCount
                                   )
                                 : nullptr;
    Pipeline* hbaoBlurDownPipeline = scene.EnableHbao
                                         ? GetOrCreatePipeline(
                                               ctx,
                                               PassKey::PostBlurDown,
                                               RHI::CullMode::None,
                                               scene.HbaoBlurDownTarget.RenderPass,
                                               scene.HbaoBlurDownTarget.ColorAttachmentCount,
                                               scene.HbaoBlurDownTarget.SampleCount
                                           )
                                         : nullptr;
    Pipeline* hbaoBlurUpPipeline = scene.EnableHbao
                                       ? GetOrCreatePipeline(
                                             ctx,
                                             PassKey::PostBlurUp,
                                             RHI::CullMode::None,
                                             scene.HbaoBlurUpTarget.RenderPass,
                                             scene.HbaoBlurUpTarget.ColorAttachmentCount,
                                             scene.HbaoBlurUpTarget.SampleCount
                                         )
                                       : nullptr;
    Pipeline* geometryPipeline = scene.EnableGeometry
                                     ? GetOrCreatePipeline(
                                           ctx,
                                           PassKey::Geometry,
                                           RHI::CullMode::Back,
                                           scene.GeometryTarget.RenderPass,
                                           scene.GeometryTarget.ColorAttachmentCount,
                                           scene.GeometryTarget.SampleCount
                                       )
                                     : nullptr;
    Pipeline* shadowVolumePipeline = scene.EnableShadowVolumes
                                         ? GetOrCreatePipeline(
                                               ctx,
                                               PassKey::ShadowVolume,
                                               RHI::CullMode::None,
                                               scene.GeometryTarget.RenderPass,
                                               scene.GeometryTarget.ColorAttachmentCount,
                                               scene.GeometryTarget.SampleCount
                                           )
                                         : nullptr;
    Pipeline* shadowVolumeMaskPipeline = scene.EnableShadowVolumes
                                             ? GetOrCreatePipeline(
                                                   ctx,
                                                   PassKey::ShadowVolumeMask,
                                                   RHI::CullMode::None,
                                                   scene.GeometryTarget.RenderPass,
                                                   scene.GeometryTarget.ColorAttachmentCount,
                                                   scene.GeometryTarget.SampleCount
                                               )
                                             : nullptr;
    Pipeline* shadowVolumeMaskClearPipeline = scene.EnableShadowVolumes
                                                  ? GetOrCreatePipeline(
                                                        ctx,
                                                        PassKey::ShadowVolumeMaskClear,
                                                        RHI::CullMode::None,
                                                        scene.GeometryTarget.RenderPass,
                                                        scene.GeometryTarget.ColorAttachmentCount,
                                                        scene.GeometryTarget.SampleCount
                                                    )
                                                  : nullptr;
    const RHI::IResourceSet* globalResources = GetOrCreateGlobalResources(ctx, scene);

    geometryPass_.SetInputs(&surface_, &scene, this, geometryPipeline, globalResources);
    shadowPass_.SetInputs(&surface_, &scene, this);
    shadowVolumePass_.SetInputs(
        &surface_,
        &scene,
        this,
        shadowVolumePipeline,
        shadowVolumeMaskClearPipeline,
        shadowVolumeMaskPipeline,
        globalResources
    );
    skyboxPass_.SetInputs(&surface_, &scene, skyboxPipeline, globalResources);
    hbaoPass_.SetInputs(&surface_, &scene, hbaoPipeline, hbaoBlurDownPipeline, hbaoBlurUpPipeline);
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
    shadowVolumePass_.RecordCommands(ctx, cmd);
    hbaoPass_.RecordCommands(ctx, cmd);
    postProcessPass_.RecordCommands(ctx, cmd);
}

void Renderer::InvalidatePipelines() {
    pipelineCache_.clear();
}

Pipeline* Renderer::GetOrCreatePipeline(
    RHI::IContext& ctx,
    const PassKey key,
    const RHI::CullMode cullMode,
    void* renderPassHandle,
    const RHI::u32 colorAttachmentCount,
    const RHI::u32 sampleCount
) {
    RHI::PipelineDesc desc{};
    desc.renderPassHandle = renderPassHandle;
    desc.colorAttachmentCount = colorAttachmentCount;
    desc.sampleCount = sampleCount;
    switch (key) {
        case PassKey::Geometry:
            desc.pipelineId = "main";
            desc.vertexLayout = RHI::VertexLayout::P3N3;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = true;
            desc.depthWrite = true;
            desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
            desc.blending = false;
            desc.cullMode = cullMode;
            desc.colorWrite = true;
            break;
        case PassKey::Shadow:
            desc.pipelineId = "shadow";
            desc.vertexLayout = RHI::VertexLayout::P3N3;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = true;
            desc.depthWrite = true;
            desc.depthCompare = RHI::DepthCompare::LessOrEqual;
            desc.blending = false;
            desc.cullMode = cullMode;
            desc.depthClamp = true;
            break;
        case PassKey::ShadowVolume:
            desc.pipelineId = "shadow_volume";
            desc.vertexLayout = RHI::VertexLayout::P3N3;
            desc.topology = RHI::PrimitiveTopology::TriangleListAdjacency;
            desc.depthTest = true;
            desc.depthWrite = false;
            desc.depthCompare = shadowVolumeDepthCompare_;
            desc.blending = false;
            desc.cullMode = shadowVolumeCullMode_;
            desc.colorWrite = false;
            desc.useColorWriteMasks = false;
            desc.depthClamp = true;
            desc.conservativeRaster = false;
            desc.stencil.Enabled = true;
            {
                const bool zPass = shadowVolumeStencilMode_ != 0;
                const bool swapOps = shadowVolumeSwapStencilOps_;
                const RHI::StencilOp inc = swapOps ? RHI::StencilOp::DecrementWrap : RHI::StencilOp::IncrementWrap;
                const RHI::StencilOp dec = swapOps ? RHI::StencilOp::IncrementWrap : RHI::StencilOp::DecrementWrap;
                desc.stencil.Front.CompareOp = RHI::StencilCompare::Always;
                desc.stencil.Back.CompareOp = RHI::StencilCompare::Always;
                desc.stencil.Front.FailOp = RHI::StencilOp::Keep;
                desc.stencil.Back.FailOp = RHI::StencilOp::Keep;
                desc.stencil.Front.Reference = 0;
                desc.stencil.Back.Reference = 0;
                desc.stencil.Front.CompareMask = 0xFFu;
                desc.stencil.Back.CompareMask = 0xFFu;
                desc.stencil.Front.WriteMask = 0xFFu;
                desc.stencil.Back.WriteMask = 0xFFu;
                if (zPass) {
                    desc.stencil.Front.PassOp = dec;
                    desc.stencil.Back.PassOp = inc;
                    desc.stencil.Front.DepthFailOp = RHI::StencilOp::Keep;
                    desc.stencil.Back.DepthFailOp = RHI::StencilOp::Keep;
                } else {
                    desc.stencil.Front.PassOp = RHI::StencilOp::Keep;
                    desc.stencil.Back.PassOp = RHI::StencilOp::Keep;
                    desc.stencil.Front.DepthFailOp = dec;
                    desc.stencil.Back.DepthFailOp = inc;
                }
            }
            break;
        case PassKey::ShadowVolumeMaskClear:
            desc.pipelineId = "shadow_volume_mask_clear";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = false;
            desc.cullMode = RHI::CullMode::None;
            desc.colorWrite = true;
            desc.stencil.Enabled = false;
            break;
        case PassKey::ShadowVolumeMask:
            desc.pipelineId = "shadow_volume_mask";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = true;
            desc.depthWrite = false;
            desc.depthCompare = shadowVolumeMaskDepthCompare_;
            desc.blending = false;
            desc.cullMode = shadowVolumeMaskCullMode_;
            desc.colorWrite = true;
            // Apply shadow mask where stencil != 0.
            desc.stencil.Enabled = true;
            desc.stencil.Front.WriteMask = 0x00u;
            desc.stencil.Back.WriteMask = 0x00u;
            desc.stencil.Front.CompareOp = RHI::StencilCompare::NotEqual;
            desc.stencil.Back.CompareOp = RHI::StencilCompare::NotEqual;
            desc.stencil.Front.Reference = 0;
            desc.stencil.Back.Reference = 0;
            desc.stencil.Front.FailOp = RHI::StencilOp::Keep;
            desc.stencil.Back.FailOp = RHI::StencilOp::Keep;
            desc.stencil.Front.PassOp = RHI::StencilOp::Keep;
            desc.stencil.Back.PassOp = RHI::StencilOp::Keep;
            desc.stencil.Front.DepthFailOp = RHI::StencilOp::Keep;
            desc.stencil.Back.DepthFailOp = RHI::StencilOp::Keep;
            break;
        case PassKey::Skybox:
            desc.pipelineId = "sky";
            desc.vertexLayout = RHI::VertexLayout::P3;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = true;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
            desc.blending = false;
            desc.cullMode = cullMode;
            desc.colorWrite = true;
            break;
        case PassKey::PostProcess:
            desc.pipelineId = "post_composite";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = true;
            desc.cullMode = cullMode;
            break;
        case PassKey::PostBlurDown:
            desc.pipelineId = "post_blur_down";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        case PassKey::PostBlurUp:
            desc.pipelineId = "post_blur_up";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        case PassKey::Hbao:
            desc.pipelineId = "post_hbao";
            desc.vertexLayout = RHI::VertexLayout::None;
            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = false;
            desc.cullMode = cullMode;
            break;
        default:
            break;
    }

    const std::size_t renderPassBits = reinterpret_cast<std::size_t>(renderPassHandle) >> 3U;
    std::uint64_t maskPack = 0ULL;
    for (std::size_t i = 0; i < desc.colorWriteMasks.size(); ++i) {
        maskPack |= (static_cast<std::uint64_t>(desc.colorWriteMasks[i]) << (i * 8ULL));
    }
    std::uint64_t stencilPack = 0ULL;
    if (desc.stencil.Enabled) {
        stencilPack |= 1ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Front.CompareOp) & 0x7ULL) << 1ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Back.CompareOp) & 0x7ULL) << 4ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Front.FailOp) & 0xFULL) << 7ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Back.FailOp) & 0xFULL) << 11ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Front.PassOp) & 0xFULL) << 15ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Back.PassOp) & 0xFULL) << 19ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Front.DepthFailOp) & 0xFULL) << 23ULL;
        stencilPack |= (static_cast<std::uint64_t>(desc.stencil.Back.DepthFailOp) & 0xFULL) << 27ULL;
        stencilPack |= static_cast<std::uint64_t>(desc.stencil.Front.CompareMask & 0xFFu) << 31ULL;
        stencilPack |= static_cast<std::uint64_t>(desc.stencil.Front.WriteMask & 0xFFu) << 39ULL;
        stencilPack |= static_cast<std::uint64_t>(desc.stencil.Front.Reference & 0xFFu) << 47ULL;
    }
    const std::size_t cacheKey =
        (static_cast<std::size_t>(key) << 8U) ^ static_cast<std::size_t>(desc.cullMode) ^
        (static_cast<std::size_t>(desc.depthCompare) << 12U) ^ (static_cast<std::size_t>(desc.depthTest) << 20U) ^
        (static_cast<std::size_t>(desc.depthWrite) << 21U) ^ (static_cast<std::size_t>(desc.blending) << 22U) ^
        (static_cast<std::size_t>(desc.colorWrite) << 23U) ^ (static_cast<std::size_t>(desc.useColorWriteMasks) << 24U) ^
        (renderPassBits << 26U) ^ (static_cast<std::size_t>(colorAttachmentCount) << 4U) ^
        (static_cast<std::size_t>(sampleCount) << 16U) ^ (static_cast<std::size_t>(maskPack) * 1099511628211ULL) ^
        (static_cast<std::size_t>(stencilPack) * 1469598103934665603ULL);

    const auto it = pipelineCache_.find(cacheKey);
    if (it != pipelineCache_.end()) {
        return it->second.get();
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
                                 (static_cast<std::size_t>(sceneColorAttachmentCount_) << 4U) |
                                 (static_cast<std::size_t>(sceneSampleCount_) << 12U);
    const auto it = pipelineCache_.find(cacheKey);
    if (it != pipelineCache_.end()) {
        return it->second.get();
    }

    RHI::PipelineDesc desc{};
    desc.pipelineId = "main";
    desc.vertexLayout = RHI::VertexLayout::P3N3;
    desc.renderPassHandle = sceneRenderPassHandle_;
    desc.colorAttachmentCount = sceneColorAttachmentCount_;
    desc.sampleCount = sceneSampleCount_;
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
