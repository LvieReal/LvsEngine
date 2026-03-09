#pragma once

#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/RHI/IContext.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering {

struct RenderSurface {
    RHI::u32 Width{0};
    RHI::u32 Height{0};
};

struct SceneData {
    struct PassTarget {
        void* RenderPass{nullptr};
        void* Framebuffer{nullptr};
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
        Common::DrawPushConstants PushConstants{};
        RHI::CullMode CullMode{RHI::CullMode::Back};
    };

    bool EnableShadows{true};
    bool EnableSkybox{true};
    bool EnablePostProcess{true};
    bool EnableGeometry{true};
    bool ClearColor{true};
    float ClearColorValue[4]{1.0F, 1.0F, 1.0F, 1.0F};
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
    const RHI::ResourceBinding* GlobalBindings{nullptr};
    RHI::u32 GlobalBindingCount{0};
    std::size_t GlobalResourceKey{0};
    const RHI::IResourceSet* GlobalResources{nullptr};
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
        const Pipeline* pipeline,
        const RHI::IResourceSet* resources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* pipeline_{nullptr};
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
        const Pipeline* pipeline,
        const RHI::IResourceSet* resources
    );

    const RenderSurface* surface_{nullptr};
    const SceneData* scene_{nullptr};
    const Pipeline* pipeline_{nullptr};
    const RHI::IResourceSet* resources_{nullptr};
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

    enum class PassKey : std::size_t {
        Shadow = 1,
        Skybox = 2,
        PostProcess = 3,
        Geometry = 4
    };

    [[nodiscard]] Pipeline* GetOrCreatePipeline(RHI::IContext& ctx, PassKey key, RHI::CullMode cullMode = RHI::CullMode::Back);
    [[nodiscard]] const RHI::IResourceSet* GetOrCreateGlobalResources(RHI::IContext& ctx, const SceneData& scene);

    RenderSurface surface_{};
    ShadowPassRenderer shadowPass_{};
    SkyboxPassRenderer skyboxPass_{};
    PostProcessPassRenderer postProcessPass_{};
    GeometryPassRenderer geometryPass_{};
    std::unordered_map<std::size_t, std::unique_ptr<Pipeline>> pipelineCache_{};
    std::unordered_map<std::size_t, std::unique_ptr<RHI::IResourceSet>> resourceSetCache_{};
};

} // namespace Lvs::Engine::Rendering
