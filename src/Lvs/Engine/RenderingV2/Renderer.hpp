#pragma once

#include "Lvs/Engine/RenderingV2/RHI/IContext.hpp"
#include "Lvs/Engine/RenderingV2/RHI/Types.hpp"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace Lvs::Engine::RenderingV2 {

struct RenderSurface {
    RHI::u32 Width{0};
    RHI::u32 Height{0};
};

struct SceneData {
    bool EnableShadows{true};
    bool EnableSkybox{true};
    bool EnablePostProcess{true};
    RHI::u32 ShadowIndexCount{0};
    RHI::u32 SkyboxIndexCount{0};
    RHI::u32 OpaqueIndexCount{0};
    RHI::u32 TransparentIndexCount{0};
    RHI::ResourceSet GlobalResources{};
};

using Pipeline = RHI::IPipeline;

class ShadowPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(const RenderSurface* surface, const SceneData* scene, const Pipeline* pipeline);

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* pipeline_{nullptr};
};

class SkyboxPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(const RenderSurface* surface, const SceneData* scene, const Pipeline* pipeline);

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* pipeline_{nullptr};
};

class PostProcessPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(const RenderSurface* surface, const SceneData* scene, const Pipeline* pipeline);

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* pipeline_{nullptr};
};

class Renderer {
public:
    void Initialize(RHI::IContext& ctx, const RenderSurface& surface);
    void RecordFrameCommands(
        RHI::IContext& ctx,
        RHI::ICommandBuffer& cmd,
        const SceneData& scene,
        RHI::u32 frameIndex
    );

private:
    enum class PassKey : std::size_t {
        Shadow = 1,
        Skybox = 2,
        PostProcess = 3
    };

    [[nodiscard]] Pipeline* GetOrCreatePipeline(RHI::IContext& ctx, PassKey key);

    RenderSurface surface_{};
    ShadowPassRenderer shadowPass_{};
    SkyboxPassRenderer skyboxPass_{};
    PostProcessPassRenderer postProcessPass_{};
    std::unordered_map<std::size_t, std::unique_ptr<Pipeline>> pipelineCache_{};
};

} // namespace Lvs::Engine::RenderingV2
