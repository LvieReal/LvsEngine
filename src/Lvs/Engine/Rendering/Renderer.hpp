#pragma once

#include "Lvs/Engine/Rendering/Common/LightBufferData.hpp"
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
    static constexpr RHI::u32 MaxDirectionalShadowMaps = Common::kMaxDirectionalShadowMaps;

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
        const RHI::IBuffer* AdjacencyIndexBuffer{nullptr};
        RHI::IndexType IndexBufferType{RHI::IndexType::UInt32};
        RHI::u32 IndexCount{0};
        RHI::IndexType AdjacencyIndexBufferType{RHI::IndexType::UInt32};
        RHI::u32 AdjacencyIndexCount{0};
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
        int ZIndex{0};
        std::array<float, 3> SortBoundsMin{};
        std::array<float, 3> SortBoundsMax{};
        bool HasSortBounds{false};
    };

    struct Image3DDrawPacket {
        const MeshRef* Mesh{nullptr};
        const RHI::IResourceSet* TextureResources{nullptr}; // descriptor set 1
        Common::Image3DPushConstants Push{};
        float SortDepth{0.0F};
        bool AlwaysOnTop{false};
    };

    bool EnableShadows{true};
    bool EnableShadowVolumes{false};
    RHI::DepthCompare ShadowVolumeDepthCompare{RHI::DepthCompare::GreaterOrEqual};
    RHI::CullMode ShadowVolumeCullMode{RHI::CullMode::None};
    RHI::DepthCompare ShadowVolumeMaskDepthCompare{RHI::DepthCompare::NotEqual};
    RHI::CullMode ShadowVolumeMaskCullMode{RHI::CullMode::None};
    int ShadowVolumeCapMode{0};
    int ShadowVolumeStencilMode{0};
    bool ShadowVolumeSwapStencilOps{false};
    bool EnableSkybox{true};
    bool EnablePostProcess{true};
    bool EnableHbao{false};
    bool EnableGeometry{true};
    bool ClearColor{true};
    float ClearColorValue[4]{1.0F, 1.0F, 1.0F, 1.0F};
    RHI::u32 DirectionalShadowCount{0};
    std::array<RHI::u32, MaxDirectionalShadowMaps> DirectionalShadowCascadeCounts{};
    std::array<std::array<PassTarget, MaxShadowCascades>, MaxDirectionalShadowMaps> DirectionalShadowCascadeTargets{};
    std::vector<DrawPacket> ShadowCasters{};
    std::array<float, 4> ShadowVolumeLightDirExtrude{}; // xyz light ray direction (world), w extrude distance
    float ShadowVolumeBias{0.0F};
    PassTarget ShadowTarget{};
    PassTarget SkyboxTarget{};
    PassTarget PostProcessTarget{};
    PassTarget GeometryTarget{};
    DrawPacket ShadowDraw{};
    DrawPacket SkyboxDraw{};
    DrawPacket PostProcessDraw{};
    DrawPacket GeometryDraw{};
    std::vector<DrawPacket> GeometryDraws{};
    std::vector<Image3DDrawPacket> Image3DDraws{};
    Common::SkyboxPushConstants SkyboxPush{};
    Common::PostCompositePushConstants PostCompositePush{};
    float NeonBlur{1.0F};
    RHI::u32 PostBlurLevelCount{0};
    PassTarget PostBlurDownTarget{};
    PassTarget PostBlurUpTarget{};
    PassTarget PostBlurFinalTarget{};
    PassTarget HbaoTarget{};
    PassTarget HbaoBlurDownTarget{};
    PassTarget HbaoBlurUpTarget{};
    PassTarget HbaoBlurFinalTarget{};
    std::array<PassTarget, MaxPostBlurLevels> PostBlurDownLevelTargets{};
    std::array<PassTarget, MaxPostBlurLevels> PostBlurUpLevelTargets{};
    std::array<PassTarget, MaxPostBlurLevels> HbaoBlurDownLevelTargets{};
    std::array<PassTarget, MaxPostBlurLevels> HbaoBlurUpLevelTargets{};
    const RHI::ResourceBinding* GlobalBindings{nullptr};
    RHI::u32 GlobalBindingCount{0};
    std::size_t GlobalResourceKey{0};
    const RHI::IResourceSet* GlobalResources{nullptr};
    std::array<const RHI::IResourceSet*, MaxDirectionalShadowMaps> DirectionalShadowResources{};
    const RHI::IResourceSet* PostBlurDownResources{nullptr};
    const RHI::IResourceSet* PostBlurUpResources{nullptr};
    const RHI::IResourceSet* PostBlurFinalResources{nullptr};
    std::array<const RHI::IResourceSet*, MaxPostBlurLevels> PostBlurDownLevelResources{};
    std::array<const RHI::IResourceSet*, MaxPostBlurLevels> PostBlurUpLevelResources{};
    const RHI::IResourceSet* PostCompositeResources{nullptr};
    float HbaoBlur{1.0F};
    RHI::u32 HbaoBlurLevelCount{0};
    const RHI::IResourceSet* HbaoResources{nullptr};
    const RHI::IResourceSet* HbaoBlurFinalResources{nullptr};
    std::array<const RHI::IResourceSet*, MaxPostBlurLevels> HbaoBlurDownLevelResources{};
    std::array<const RHI::IResourceSet*, MaxPostBlurLevels> HbaoBlurUpLevelResources{};
    Common::HbaoPushConstants HbaoPush{};
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
        Renderer* renderer
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    Renderer* renderer_{nullptr};
};

class ShadowVolumePassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        Renderer* renderer,
        const Pipeline* volumePipeline,
        const Pipeline* applyPipeline,
        const RHI::IResourceSet* globalResources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    Renderer* renderer_{nullptr};
    const Pipeline* volumePipeline_{nullptr};
    const Pipeline* applyPipeline_{nullptr};
    const RHI::IResourceSet* globalResources_{nullptr};
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

class HbaoPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        const Pipeline* hbaoPipeline,
        const Pipeline* blurDownPipeline,
        const Pipeline* blurUpPipeline
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* hbaoPipeline_{nullptr};
    const Pipeline* blurDownPipeline_{nullptr};
    const Pipeline* blurUpPipeline_{nullptr};
};

class GeometryPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);
    enum class Phase {
        All,
        OpaqueOnly,
        TransparentOnly
    };
    void SetPhase(Phase phase) { phase_ = phase; }

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
    Phase phase_{Phase::All};
};

class Image3DPassRenderer {
public:
    void RecordCommands(RHI::IContext& ctx, RHI::ICommandBuffer& cmd);

private:
    friend class Renderer;
    void SetInputs(
        const RenderSurface* surface,
        const SceneData* scene,
        Renderer* renderer,
        const RHI::IResourceSet* globalResources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    Renderer* renderer_{nullptr};
    const RHI::IResourceSet* globalResources_{nullptr};
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
    void InvalidatePipelines();

private:
    friend class GeometryPassRenderer;
    friend class ShadowPassRenderer;
    friend class Image3DPassRenderer;

    enum class PassKey : std::size_t {
        Shadow = 1,
        ShadowVolume = 8,
        ShadowVolumeMaskClear = 10,
        ShadowVolumeMask = 9,
        ShadowVolumeApply = 11,
        Skybox = 2,
        PostProcess = 3,
        Geometry = 4,
        PostBlurDown = 5,
        PostBlurUp = 6,
        Hbao = 7,
        Image3D = 12
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
    [[nodiscard]] Pipeline* GetOrCreateImage3DPipeline(
        RHI::IContext& ctx,
        bool alwaysOnTop
    );
    [[nodiscard]] const RHI::IResourceSet* GetOrCreateGlobalResources(RHI::IContext& ctx, const SceneData& scene);

    RenderSurface surface_{};
    // Per-frame overrides (currently used for shadow-volume tuning via Lighting service).
    RHI::DepthCompare shadowVolumeDepthCompare_{RHI::DepthCompare::GreaterOrEqual};
    RHI::CullMode shadowVolumeCullMode_{RHI::CullMode::None};
    RHI::DepthCompare shadowVolumeMaskDepthCompare_{RHI::DepthCompare::NotEqual};
    RHI::CullMode shadowVolumeMaskCullMode_{RHI::CullMode::None};
    int shadowVolumeStencilMode_{0};
    bool shadowVolumeSwapStencilOps_{false};
    ShadowPassRenderer shadowPass_{};
    ShadowVolumePassRenderer shadowVolumePass_{};
    SkyboxPassRenderer skyboxPass_{};
    HbaoPassRenderer hbaoPass_{};
    PostProcessPassRenderer postProcessPass_{};
    GeometryPassRenderer geometryPass_{};
    Image3DPassRenderer image3dPass_{};
    void* sceneRenderPassHandle_{nullptr};
    RHI::u32 sceneColorAttachmentCount_{1};
    RHI::u32 sceneSampleCount_{1};
    std::unordered_map<std::size_t, std::unique_ptr<Pipeline>> pipelineCache_{};
    std::unordered_map<std::size_t, std::unique_ptr<RHI::IResourceSet>> resourceSetCache_{};
};

} // namespace Lvs::Engine::Rendering
