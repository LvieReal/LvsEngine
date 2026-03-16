#pragma once

#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/RHI/IContext.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <cstddef>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering {

struct RenderSurface {
    RHI::u32 Width{0};
    RHI::u32 Height{0};
};

struct SceneData {
    static constexpr RHI::u32 MaxPostBlurLevels = 4U;
    static constexpr RHI::u32 MaxShadowCascades = 3U;

    struct PassTarget {
        void* RenderPass{nullptr};
        void* Framebuffer{nullptr};
        RHI::u32 ColorAttachmentCount{1};
        RHI::u32 SampleCount{1};
        RHI::u32 Width{0};
        RHI::u32 Height{0};
    };

    struct MeshRef {
        const RHI::IBuffer* VertexBuffer{nullptr};
        const RHI::IBuffer* IndexBuffer{nullptr};
        RHI::IndexType IndexBufferType{RHI::IndexType::UInt32};
        RHI::u32 IndexCount{0};
        std::size_t VertexOffset{0};
        std::size_t IndexOffset{0};
    };

    struct DrawPacket {
        const MeshRef* Mesh{nullptr};
        RHI::u32 BaseInstance{0};
        RHI::u32 InstanceCount{1};
        RHI::CullMode CullMode{RHI::CullMode::Back};
        bool Transparent{false};
        bool AlwaysOnTop{false};
        bool IgnoreLighting{false};
        float SortDepth{0.0F};
    };

    bool EnableShadows{true};
    bool EnableSkybox{true};
    bool EnablePostProcess{true};
    bool EnableGeometry{true};
    bool ClearColor{true};
    float ClearColorValue[4]{1.0F, 1.0F, 1.0F, 1.0F};
    RHI::u32 ShadowCascadeCount{0};
    std::array<PassTarget, MaxShadowCascades> ShadowCascadeTargets{};
    std::vector<DrawPacket> ShadowCasters{};
    PassTarget ShadowTarget{};
    PassTarget SkyboxTarget{};
    PassTarget PostProcessTarget{};
    PassTarget GeometryTarget{};
    DrawPacket ShadowDraw{};
    DrawPacket SkyboxDraw{};
    DrawPacket PostProcessDraw{};
    DrawPacket GeometryDraw{};
    std::vector<DrawPacket> GeometryDraws{};
    Common::SkyboxPushConstants SkyboxPush{};
    Common::PostProcessPushConstants PostProcessPush{};
    float NeonBlur{1.0F};
    RHI::u32 PostBlurLevelCount{0};
    PassTarget PostBlurDownTarget{};
    PassTarget PostBlurUpTarget{};
    PassTarget PostBlurFinalTarget{};
    std::array<PassTarget, MaxPostBlurLevels> PostBlurDownLevelTargets{};
    std::array<PassTarget, MaxPostBlurLevels> PostBlurUpLevelTargets{};
    const RHI::ResourceBinding* GlobalBindings{nullptr};
    RHI::u32 GlobalBindingCount{0};
    std::size_t GlobalResourceKey{0};
    const RHI::IResourceSet* GlobalResources{nullptr};
    const RHI::IResourceSet* ShadowResources{nullptr};
    const RHI::IResourceSet* PostBlurDownResources{nullptr};
    const RHI::IResourceSet* PostBlurUpResources{nullptr};
    const RHI::IResourceSet* PostBlurFinalResources{nullptr};
    std::array<const RHI::IResourceSet*, MaxPostBlurLevels> PostBlurDownLevelResources{};
    std::array<const RHI::IResourceSet*, MaxPostBlurLevels> PostBlurUpLevelResources{};
    const RHI::IResourceSet* PostCompositeResources{nullptr};
};

using Pipeline = RHI::IPipeline;
class Renderer;

class ShadowPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        Renderer* renderer,
        const RHI::IResourceSet* resources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    Renderer* renderer_{nullptr};
    const RHI::IResourceSet* resources_{nullptr};
};

class SkyboxPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        const Pipeline* pipeline,
        const RHI::IResourceSet* resources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* pipeline_{nullptr};
    const RHI::IResourceSet* resources_{nullptr};
};

class PostProcessPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        const Pipeline* compositePipeline,
        const Pipeline* blurDownPipeline,
        const Pipeline* blurUpPipeline
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* compositePipeline_{nullptr};
    const Pipeline* blurDownPipeline_{nullptr};
    const Pipeline* blurUpPipeline_{nullptr};
};

class GeometryPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        Renderer* renderer,
        const Pipeline* pipeline,
        const RHI::IResourceSet* resources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    Renderer* renderer_{nullptr};
    const Pipeline* pipeline_{nullptr};
    const RHI::IResourceSet* resources_{nullptr};
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
    friend class GeometryPassRenderer;
    friend class ShadowPassRenderer;

    enum class PassKey : std::size_t {
        Shadow = 1,
        Skybox = 2,
        PostProcess = 3,
        Geometry = 4,
        PostBlurDown = 5,
        PostBlurUp = 6
    };

    [[nodiscard]] Pipeline* GetOrCreatePipeline(
        RHI::IContext& ctx,
        PassKey key,
        RHI::CullMode cullMode,
        void* renderPassHandle,
        RHI::u32 colorAttachmentCount,
        RHI::u32 sampleCount
    );
    [[nodiscard]] Pipeline* GetOrCreateGeometryPipeline(
        RHI::IContext& ctx,
        RHI::CullMode cullMode,
        bool transparent,
        bool alwaysOnTop
    );
    [[nodiscard]] const RHI::IResourceSet* GetOrCreateGlobalResources(RHI::IContext& ctx, const SceneData& scene);

    RenderSurface surface_{};
    ShadowPassRenderer shadowPass_{};
    SkyboxPassRenderer skyboxPass_{};
    PostProcessPassRenderer postProcessPass_{};
    GeometryPassRenderer geometryPass_{};
    void* sceneRenderPassHandle_{nullptr};
    RHI::u32 sceneColorAttachmentCount_{1};
    RHI::u32 sceneSampleCount_{1};
    std::unordered_map<std::size_t, std::unique_ptr<Pipeline>> pipelineCache_{};
    std::unordered_map<std::size_t, std::unique_ptr<RHI::IResourceSet>> resourceSetCache_{};
};

} // namespace Lvs::Engine::Rendering
