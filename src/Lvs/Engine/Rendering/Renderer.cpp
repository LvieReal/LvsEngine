#include "Lvs/Engine/Rendering/Renderer.hpp"

namespace Lvs::Engine::Rendering {

void Renderer::Initialize(RHI::IContext& ctx, const RenderSurface& surface) {
    surface_ = surface;
    static_cast<void>(GetOrCreatePipeline(ctx, PassKey::Geometry, RHI::CullMode::Back));
}

void Renderer::RecordFrameCommands(
    RHI::IContext& ctx,
    RHI::ICommandBuffer& cmd,
    const SceneData& scene,
    const RHI::u32 frameIndex
) {
    static_cast<void>(frameIndex);

    if (scene.ClearColor) {
        const RHI::RenderPassInfo clearPass{
            .width = surface_.Width,
            .height = surface_.Height,
            .renderPassHandle = scene.PostProcessTarget.RenderPass,
            .framebufferHandle = scene.PostProcessTarget.Framebuffer,
            .clearColor = true,
            .clearColorValue = {
                scene.ClearColorValue[0],
                scene.ClearColorValue[1],
                scene.ClearColorValue[2],
                scene.ClearColorValue[3]
            },
            .clearDepth = false,
            .clearDepthValue = 0.0F
        };
        cmd.BeginRenderPass(clearPass);
        cmd.EndRenderPass();
    }

    Pipeline* shadowPipeline = scene.EnableShadows ? GetOrCreatePipeline(ctx, PassKey::Shadow, RHI::CullMode::Back) : nullptr;
    Pipeline* skyboxPipeline = scene.EnableSkybox ? GetOrCreatePipeline(ctx, PassKey::Skybox, RHI::CullMode::Front) : nullptr;
    Pipeline* postProcessPipeline =
        scene.EnablePostProcess ? GetOrCreatePipeline(ctx, PassKey::PostProcess, RHI::CullMode::None) : nullptr;
    Pipeline* geometryPipeline = scene.EnableGeometry ? GetOrCreatePipeline(ctx, PassKey::Geometry, RHI::CullMode::Back) : nullptr;
    const RHI::IResourceSet* globalResources = GetOrCreateGlobalResources(ctx, scene);

    geometryPass_.SetInputs(&surface_, &scene, this, geometryPipeline, globalResources);
    shadowPass_.SetInputs(&surface_, &scene, shadowPipeline, globalResources);
    skyboxPass_.SetInputs(&surface_, &scene, skyboxPipeline, globalResources);
    postProcessPass_.SetInputs(&surface_, &scene, postProcessPipeline, globalResources);

    geometryPass_.RecordCommands(ctx, cmd);
    shadowPass_.RecordCommands(ctx, cmd);
    skyboxPass_.RecordCommands(ctx, cmd);
    postProcessPass_.RecordCommands(ctx, cmd);
}

Pipeline* Renderer::GetOrCreatePipeline(RHI::IContext& ctx, const PassKey key, const RHI::CullMode cullMode) {
    const std::size_t cacheKey = (static_cast<std::size_t>(key) << 8U) | static_cast<std::size_t>(cullMode);
    const auto it = pipelineCache_.find(cacheKey);
    if (it != pipelineCache_.end()) {
        return it->second.get();
    }

    RHI::PipelineDesc desc{};
    switch (key) {
        case PassKey::Geometry:
            desc.pipelineId = "mesh";
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
            desc.depthCompare = RHI::DepthCompare::GreaterOrEqual;
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
            desc.vertexLayout = RHI::VertexLayout::P3N3;
            desc.depthTest = false;
            desc.depthWrite = false;
            desc.depthCompare = RHI::DepthCompare::Always;
            desc.blending = true;
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
