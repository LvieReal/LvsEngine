#pragma once

#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/GLApi.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanApi.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowCascadeUtils.hpp"
#include "Lvs/Engine/Rendering/Common/LightBufferData.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxSettingsResolver.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/Rendering/Renderer.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::Vulkan {
class VulkanContext;
}

namespace Lvs::Engine::Rendering::Backends::OpenGL {
class GLContext;
}

namespace Lvs::Engine::Rendering::Common {
struct MeshData;
}

namespace Lvs::Engine::Rendering {

class RenderContext final : public IRenderContext {
public:
    explicit RenderContext(RenderApi preferredApi);
    ~RenderContext() override;

    void Initialize(RHI::u32 width, RHI::u32 height) override;
    void AttachToNativeWindow(void* nativeWindowHandle, RHI::u32 width, RHI::u32 height) override;
    void Resize(RHI::u32 width, RHI::u32 height) override;
    void SetClearColor(float r, float g, float b, float a) override;
    void BindToPlace(const std::shared_ptr<DataModel::Place>& place) override;
    void Unbind() override;
    void SetOverlayPrimitives(std::vector<Common::OverlayPrimitive> primitives) override;
    void RefreshShaders() override;
    void Render() override;

private:
    struct GpuMesh {
        std::unique_ptr<RHI::IBuffer> VertexBuffer{};
        std::unique_ptr<RHI::IBuffer> IndexBuffer{};
        RHI::u32 IndexCount{0};
        RHI::IndexType IndexType{RHI::IndexType::UInt32};
    };

    struct WatchedNode {
        std::weak_ptr<Core::Instance> Instance{};
        Core::Instance::InstanceConnection ChildAdded{};
        Core::Instance::InstanceConnection ChildRemoved{};
        Utils::Signal<>::Connection Destroying{};
    };

    struct CachedRenderable {
        std::weak_ptr<Core::Instance> Instance{};
        Core::Instance::PropertyChangedConnection PropertyChanged{};
        Core::Instance::InstanceConnection AncestryChanged{};
        Utils::Signal<>::Connection Destroying{};

        bool UnderWorkspace{false};
        bool Visible{false};
        bool Transparent{false};
        bool AlwaysOnTop{false};
        bool IgnoreLighting{false};
        float Alpha{1.0F};
        float SortDepth{0.0F};
        RHI::CullMode CullMode{RHI::CullMode::Back};
        const SceneData::MeshRef* Mesh{nullptr};
        std::string MeshKey{};

        bool LayoutDirty{true};
        bool DataDirty{true};
        std::size_t PackedInstanceIndex{0};

        Common::DrawInstanceData InstanceData{};
    };

    struct BatchKey {
        const SceneData::MeshRef* Mesh{nullptr};
        RHI::CullMode CullMode{RHI::CullMode::Back};
        bool AlwaysOnTop{false};

        friend bool operator==(const BatchKey& lhs, const BatchKey& rhs) {
            return lhs.Mesh == rhs.Mesh && lhs.CullMode == rhs.CullMode && lhs.AlwaysOnTop == rhs.AlwaysOnTop;
        }
    };

    struct BatchKeyHash {
        std::size_t operator()(const BatchKey& key) const noexcept {
            const std::size_t ptrHash = std::hash<const void*>{}(key.Mesh);
            const std::size_t cullHash = std::hash<std::size_t>{}(static_cast<std::size_t>(key.CullMode));
            const std::size_t topHash = std::hash<bool>{}(key.AlwaysOnTop);
            return ptrHash ^ (cullHash << 1U) ^ (topHash << 2U);
        }
    };

    void WaitForBackendIdle();
    void ReleaseGpuResources();
    void EnsureDirectionalShadowTargets(RHI::u32 shadowIndex, const Common::ShadowSettings& settings);
    void EnsureFallbackShadowTarget();
    void EnsureShadowJitterTexture();
    void EnsurePostProcessTargets();
    void EnsureFallbackTextures();
    void EnsureBackend();
    RHI::IContext& GetRhiContext();
    [[nodiscard]] std::optional<GpuMesh> CreateGpuMeshFromData(const Common::MeshData& mesh);
    void InitializeGeometryBuffers();
    GpuMesh* GetOrCreatePrimitiveMesh(Enums::PartShape shape);
    GpuMesh* GetOrCreateMeshPartMesh(const std::string& contentId, bool smoothNormals);
    void TrimRetiredFrameResources();
    void UpdateSkyboxTexture();
    void UpdateSurfaceAtlasTexture();
    void UpdateSurfaceNormalAtlasTexture();
    void ClearGeometryCache();
    void EnsureGeometryCache();
    void WatchNodeRecursive(const std::shared_ptr<Core::Instance>& node);
    void UnwatchNodeRecursive(const std::shared_ptr<Core::Instance>& node);
    void TrackRenderable(const std::shared_ptr<Core::Instance>& instance);
    void UntrackRenderable(const Core::Instance* instance);
    void MarkGeometryLayoutDirty();
    void MarkGeometryDataDirty();
    [[nodiscard]] bool IsUnderWorkspace(const std::shared_ptr<Core::Instance>& instance) const;
    [[nodiscard]] const SceneData::MeshRef* GetOrCreateMeshRef(const std::string& key, const GpuMesh& mesh);
    void RebuildGeometryBatchesAndInstances();
    void RebuildOverlayBatchesAndInstances();
    void UpdateDirtyInstanceData();
    void UpdateTransparentSortDepths();
    [[nodiscard]] std::vector<SceneData::DrawPacket> BuildGeometryDraws();
    [[nodiscard]] Common::CameraUniformData BuildCameraUniforms();
    [[nodiscard]] Common::SkyboxPushConstants BuildSkyboxPushConstants() const;

    RenderApi preferredApi_{RenderApi::Auto};
    RenderApi activeApi_{RenderApi::Auto};

    Backends::Vulkan::VulkanApi vkApi_{};
    Backends::OpenGL::GLApi glApi_{};

    std::unique_ptr<Backends::Vulkan::VulkanContext> vkBackend_{};
    std::unique_ptr<Backends::OpenGL::GLContext> glBackend_{};

    void* nativeWindowHandle_{nullptr};

    std::shared_ptr<DataModel::Place> place_{};

    std::vector<Common::OverlayPrimitive> overlayPrimitives_{};
    std::size_t overlayCacheKey_{0};
    bool overlayDirty_{true};

    std::unordered_map<Enums::PartShape, GpuMesh> primitiveMeshCache_{};
    std::unordered_map<std::string, GpuMesh> meshPartCache_{};
    std::unordered_map<std::string, const SceneData::MeshRef*> meshRefCache_{};
    std::deque<SceneData::MeshRef> meshRefStorage_{};

    std::weak_ptr<Core::Instance> workspaceRoot_{};
    std::unordered_map<const Core::Instance*, WatchedNode> watchedNodes_{};
    std::unordered_map<const Core::Instance*, CachedRenderable> renderables_{};

    bool geometryLayoutDirty_{true};
    bool geometryDataDirty_{true};
    bool instanceBufferDirty_{true};
    bool geometryCacheInitialized_{false};

    std::vector<Common::DrawInstanceData> cachedInstanceData_{};
    std::vector<SceneData::DrawPacket> cachedOpaqueDraws_{};
    std::vector<SceneData::DrawPacket> cachedTransparentDraws_{};
    std::vector<SceneData::DrawPacket> cachedAlwaysOnTopDraws_{};

    std::size_t cachedGeometryInstanceCount_{0};
    std::size_t cachedGeometryOpaqueDrawCount_{0};
    std::size_t cachedGeometryTransparentDrawCount_{0};
    std::size_t cachedGeometryAlwaysOnTopDrawCount_{0};

    std::unique_ptr<RHI::IBuffer> frameUniformBuffer_{};
    std::unique_ptr<RHI::IResourceSet> frameResourceSet_{};
    std::array<std::unique_ptr<RHI::IBuffer>, Common::kMaxDirectionalShadowMaps> frameShadowUniformBuffers_{};
    std::array<std::unique_ptr<RHI::IResourceSet>, Common::kMaxDirectionalShadowMaps> frameShadowResourceSets_{};
    std::unique_ptr<RHI::IBuffer> frameInstanceBuffer_{};
    std::unique_ptr<RHI::IBuffer> frameLightBuffer_{};

    std::array<std::unique_ptr<RHI::IResourceSet>, SceneData::MaxPostBlurLevels> postBlurDownLevelResourceSets_{};
    std::array<std::unique_ptr<RHI::IResourceSet>, SceneData::MaxPostBlurLevels> postBlurUpLevelResourceSets_{};
    std::unique_ptr<RHI::IResourceSet> postBlurFinalResourceSet_{};
    std::unique_ptr<RHI::IResourceSet> postCompositeResourceSet_{};

    std::unique_ptr<RHI::IRenderTarget> geometryTarget_{};

    std::array<std::unique_ptr<RHI::IRenderTarget>, SceneData::MaxPostBlurLevels> blurDownTargets_{};
    std::array<std::unique_ptr<RHI::IRenderTarget>, SceneData::MaxPostBlurLevels> blurUpTargets_{};
    std::unique_ptr<RHI::IRenderTarget> blurFinalTarget_{};

    std::array<std::array<std::unique_ptr<RHI::IRenderTarget>, SceneData::MaxShadowCascades>, Common::kMaxDirectionalShadowMaps>
        directionalShadowTargets_{};
    std::unique_ptr<RHI::IRenderTarget> fallbackShadowTarget_{};

    std::deque<std::unique_ptr<RHI::IBuffer>> retiredFrameUniformBuffers_{};
    std::deque<std::unique_ptr<RHI::IResourceSet>> retiredFrameResourceSets_{};

    Common::SkyboxSettingsResolver skyboxResolver_{};

    std::array<Common::ShadowSettings, Common::kMaxDirectionalShadowMaps> directionalShadowSettings_{};
    std::array<Common::ShadowCascadeComputation, Common::kMaxDirectionalShadowMaps> directionalShadowCascadeComputations_{};
    std::array<std::array<RHI::u32, Common::kMaxShadowCascades>, Common::kMaxDirectionalShadowMaps>
        directionalShadowCascadeResolutions_{};

    std::optional<std::size_t> skyboxSettingsKey_{};
    Math::Color3 skyboxTint_{1.0, 1.0, 1.0};

    RHI::Texture surfaceAtlas_{};
    bool hasSurfaceAtlas_{false};

    RHI::Texture surfaceNormalAtlas_{};
    bool hasSurfaceNormalAtlas_{false};

    RHI::Texture shadowJitterTexture_{};
    bool hasShadowJitterTexture_{false};
    float shadowJitterScaleXY_{1.0F / 16.0F};

    RHI::Texture skyboxCubemap_{};
    bool hasSkyboxCubemap_{false};

    RHI::Texture fallbackBlackTexture_{};
    bool hasFallbackBlackTexture_{false};

    std::uint32_t postProcessFrameSeed_{0};
    bool refreshShadersRequested_{false};

    RHI::u32 surfaceWidth_{0};
    RHI::u32 surfaceHeight_{0};
    RHI::u32 requestedMsaaSampleCount_{1};
    RHI::u32 effectiveMsaaSampleCount_{1};
    bool requestedSurfaceMipmaps_{true};

    float clearColor_[4]{1.0F, 1.0F, 1.0F, 1.0F};
};

} // namespace Lvs::Engine::Rendering
