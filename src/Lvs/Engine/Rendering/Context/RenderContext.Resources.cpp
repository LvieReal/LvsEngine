#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/Rendering/Common/CubemapLoader.hpp"
#include "Lvs/Engine/Rendering/Common/MeshData.hpp"
#include "Lvs/Engine/Rendering/Common/MeshLoader.hpp"
#include "Lvs/Engine/Rendering/Common/Primitives.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowJitterUtils.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"
#include "Lvs/Engine/Utils/ImageIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <optional>
#include <utility>

namespace Lvs::Engine::Rendering {

void RenderContext::ReleaseGpuResources() {
    if (hasSurfaceAtlas_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
        GetRhiContext().DestroyTexture(surfaceAtlas_);
        surfaceAtlas_ = {};
        hasSurfaceAtlas_ = false;
    }
    if (hasSurfaceNormalAtlas_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
        GetRhiContext().DestroyTexture(surfaceNormalAtlas_);
        surfaceNormalAtlas_ = {};
        hasSurfaceNormalAtlas_ = false;
    }
    if (hasShadowJitterTexture_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
        GetRhiContext().DestroyTexture(shadowJitterTexture_);
        shadowJitterTexture_ = {};
        hasShadowJitterTexture_ = false;
    }
    if (hasFallbackBlackTexture_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
        GetRhiContext().DestroyTexture(fallbackBlackTexture_);
        fallbackBlackTexture_ = {};
        hasFallbackBlackTexture_ = false;
    }
    if (hasFallbackWhiteTexture_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
        GetRhiContext().DestroyTexture(fallbackWhiteTexture_);
        fallbackWhiteTexture_ = {};
        hasFallbackWhiteTexture_ = false;
    }
    if (hasSkyboxCubemap_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
        GetRhiContext().DestroyTexture(skyboxCubemap_);
        skyboxCubemap_ = {};
        hasSkyboxCubemap_ = false;
    }
    frameResourceSet_.reset();
    for (auto& set : postBlurDownLevelResourceSets_) {
        set.reset();
    }
    for (auto& set : postBlurUpLevelResourceSets_) {
        set.reset();
    }
    postBlurFinalResourceSet_.reset();
    postCompositeResourceSet_.reset();
    hbaoResourceSet_.reset();
    for (auto& set : hbaoBlurDownLevelResourceSets_) {
        set.reset();
    }
    for (auto& set : hbaoBlurUpLevelResourceSets_) {
        set.reset();
    }
    hbaoBlurFinalResourceSet_.reset();
    for (auto& set : frameShadowResourceSets_) {
        set.reset();
    }
    frameUniformBuffer_.reset();
    for (auto& buf : frameShadowUniformBuffers_) {
        buf.reset();
    }
    frameInstanceBuffer_.reset();
    frameLightBuffer_.reset();
    retiredFrameResourceSets_.clear();
    retiredFrameUniformBuffers_.clear();
    primitiveMeshCache_.clear();
    meshPartCache_.clear();
    meshRefCache_.clear();
    meshRefStorage_.clear();
    ClearGeometryCache();
    geometryTarget_.reset();
    for (auto& target : blurDownTargets_) {
        target.reset();
    }
    for (auto& target : blurUpTargets_) {
        target.reset();
    }
    blurFinalTarget_.reset();
    hbaoTarget_.reset();
    for (auto& target : hbaoBlurDownTargets_) {
        target.reset();
    }
    for (auto& target : hbaoBlurUpTargets_) {
        target.reset();
    }
    hbaoBlurFinalTarget_.reset();
    for (auto& perLightTargets : directionalShadowTargets_) {
        for (auto& target : perLightTargets) {
            target.reset();
        }
    }
    fallbackShadowTarget_.reset();
    skyboxSettingsKey_.reset();
}

void RenderContext::EnsureDirectionalShadowTargets(const RHI::u32 shadowIndex, const Common::ShadowSettings& settings) {
    if (shadowIndex >= Common::kMaxDirectionalShadowMaps) {
        return;
    }
    EnsureFallbackShadowTarget();
    EnsureBackend();
    const Common::ShadowSettings normalized = Common::NormalizeShadowSettings(settings);
    directionalShadowSettings_[shadowIndex] = normalized;
    directionalShadowCascadeResolutions_[shadowIndex] = Common::ComputeCascadeResolutions(
        normalized.MapResolution,
        normalized.CascadeResolutionScale
    );

    for (RHI::u32 i = 0; i < SceneData::MaxShadowCascades; ++i) {
        const RHI::u32 desired = directionalShadowCascadeResolutions_[shadowIndex][i];
        const auto needsRecreate = [&](const std::unique_ptr<RHI::IRenderTarget>& target) {
            return target == nullptr || target->GetWidth() != desired || target->GetHeight() != desired;
        };
        if (i < static_cast<RHI::u32>(normalized.CascadeCount) && needsRecreate(directionalShadowTargets_[shadowIndex][i])) {
            directionalShadowTargets_[shadowIndex][i] = GetRhiContext().CreateRenderTarget(RHI::RenderTargetDesc{
                .width = desired,
                .height = desired,
                .colorAttachmentCount = 0,
                .hasDepth = true,
                .depthTexture = true,
                .depthFormat = RHI::Format::D32_Float
            });
        }
    }
}

void RenderContext::EnsureFallbackShadowTarget() {
    EnsureBackend();
    if (fallbackShadowTarget_ != nullptr) {
        return;
    }
    fallbackShadowTarget_ = GetRhiContext().CreateRenderTarget(RHI::RenderTargetDesc{
        .width = 1,
        .height = 1,
        .colorAttachmentCount = 0,
        .hasDepth = true,
        .depthTexture = true,
        .depthFormat = RHI::Format::D32_Float
    });
}

void RenderContext::EnsureShadowJitterTexture() {
    if (hasShadowJitterTexture_) {
        return;
    }
    EnsureBackend();
    const std::uint32_t sizeXY = Common::kShadowDefaultJitterSizeXY;
    const std::uint32_t depth = Common::kShadowDefaultJitterDepth;
    const auto data = Common::GenerateShadowJitterTextureData(sizeXY, depth);
    if (data.empty()) {
        return;
    }
    RHI::Texture3DDesc desc{};
    desc.width = static_cast<RHI::u32>(sizeXY);
    desc.height = static_cast<RHI::u32>(sizeXY);
    desc.depth = static_cast<RHI::u32>(depth);
    desc.format = RHI::Format::R8G8B8A8_UNorm;
    desc.linearFiltering = false;
    desc.repeat = true;
    desc.pixels = data;
    shadowJitterTexture_ = GetRhiContext().CreateTexture3D(desc);
    hasShadowJitterTexture_ = shadowJitterTexture_.graphic_handle_ptr != nullptr;
    shadowJitterScaleXY_ = 1.0F / static_cast<float>(std::max<std::uint32_t>(1U, sizeXY));
}

void RenderContext::EnsurePostProcessTargets() {
    if (surfaceWidth_ == 0U || surfaceHeight_ == 0U) {
        return;
    }
    const auto needsRecreate = [this](const std::unique_ptr<RHI::IRenderTarget>& target,
                                      const RHI::u32 width,
                                      const RHI::u32 height,
                                      const RHI::u32 colors,
                                      const RHI::u32 sampleCount) {
        return target == nullptr || target->GetWidth() != width || target->GetHeight() != height ||
               target->GetColorAttachmentCount() != colors || target->GetSampleCount() != sampleCount;
    };

    bool waitedForIdle = false;
    const auto waitForIdleOnce = [this, &waitedForIdle]() {
        if (waitedForIdle) {
            return;
        }
        WaitForBackendIdle();
        waitedForIdle = true;
    };
    if (needsRecreate(geometryTarget_, surfaceWidth_, surfaceHeight_, 3U, effectiveMsaaSampleCount_)) {
        waitForIdleOnce();
        geometryTarget_ = GetRhiContext().CreateRenderTarget(
            RHI::RenderTargetDesc{
                .width = surfaceWidth_,
                .height = surfaceHeight_,
                .colorAttachmentCount = 3,
                .sampleCount = requestedMsaaSampleCount_,
                .hasDepth = true,
                .depthTexture = false,
                .depthCompare = false
            }
        );
        if (geometryTarget_ != nullptr) {
            effectiveMsaaSampleCount_ = geometryTarget_->GetSampleCount();
        }
    }

    RHI::u32 levelWidth = std::max<RHI::u32>(1U, surfaceWidth_ / 2U);
    RHI::u32 levelHeight = std::max<RHI::u32>(1U, surfaceHeight_ / 2U);
    for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
        if (needsRecreate(blurDownTargets_[level], levelWidth, levelHeight, 1U, 1U)) {
            waitForIdleOnce();
            blurDownTargets_[level] = GetRhiContext().CreateRenderTarget(
                RHI::RenderTargetDesc{
                    .width = levelWidth,
                    .height = levelHeight,
                    .colorAttachmentCount = 1,
                    .hasDepth = false
                }
            );
        }
        if (needsRecreate(blurUpTargets_[level], levelWidth, levelHeight, 1U, 1U)) {
            waitForIdleOnce();
            blurUpTargets_[level] = GetRhiContext().CreateRenderTarget(
                RHI::RenderTargetDesc{
                    .width = levelWidth,
                    .height = levelHeight,
                    .colorAttachmentCount = 1,
                    .hasDepth = false
                }
            );
        }
        levelWidth = std::max<RHI::u32>(1U, levelWidth / 2U);
        levelHeight = std::max<RHI::u32>(1U, levelHeight / 2U);
    }
    if (needsRecreate(blurFinalTarget_, surfaceWidth_, surfaceHeight_, 1U, 1U)) {
        waitForIdleOnce();
        blurFinalTarget_ = GetRhiContext().CreateRenderTarget(
            RHI::RenderTargetDesc{
                .width = surfaceWidth_,
                .height = surfaceHeight_,
                .colorAttachmentCount = 1,
                .hasDepth = false
            }
        );
    }
}

void RenderContext::EnsureHbaoTargets() {
    if (surfaceWidth_ == 0U || surfaceHeight_ == 0U) {
        return;
    }
    EnsureBackend();

    const RHI::u32 baseWidth = std::max<RHI::u32>(1U, surfaceWidth_ / 2U);
    const RHI::u32 baseHeight = std::max<RHI::u32>(1U, surfaceHeight_ / 2U);

    const auto needsRecreate =
        [](const std::unique_ptr<RHI::IRenderTarget>& target, const RHI::u32 width, const RHI::u32 height) {
            return target == nullptr || target->GetWidth() != width || target->GetHeight() != height;
        };

    bool waitedForIdle = false;
    const auto waitForIdleOnce = [this, &waitedForIdle]() {
        if (waitedForIdle) {
            return;
        }
        WaitForBackendIdle();
        waitedForIdle = true;
    };

    if (needsRecreate(hbaoTarget_, baseWidth, baseHeight)) {
        waitForIdleOnce();
        hbaoTarget_ = GetRhiContext().CreateRenderTarget(
            RHI::RenderTargetDesc{
                .width = baseWidth,
                .height = baseHeight,
                .colorAttachmentCount = 1,
                .sampleCount = 1,
                .hasDepth = false
            }
        );
    }

    RHI::u32 levelWidth = baseWidth;
    RHI::u32 levelHeight = baseHeight;
    for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
        if (needsRecreate(hbaoBlurDownTargets_[level], levelWidth, levelHeight)) {
            waitForIdleOnce();
            hbaoBlurDownTargets_[level] = GetRhiContext().CreateRenderTarget(
                RHI::RenderTargetDesc{
                    .width = levelWidth,
                    .height = levelHeight,
                    .colorAttachmentCount = 1,
                    .sampleCount = 1,
                    .hasDepth = false
                }
            );
        }
        if (needsRecreate(hbaoBlurUpTargets_[level], levelWidth, levelHeight)) {
            waitForIdleOnce();
            hbaoBlurUpTargets_[level] = GetRhiContext().CreateRenderTarget(
                RHI::RenderTargetDesc{
                    .width = levelWidth,
                    .height = levelHeight,
                    .colorAttachmentCount = 1,
                    .sampleCount = 1,
                    .hasDepth = false
                }
            );
        }
        levelWidth = std::max<RHI::u32>(1U, levelWidth / 2U);
        levelHeight = std::max<RHI::u32>(1U, levelHeight / 2U);
    }

    if (needsRecreate(hbaoBlurFinalTarget_, baseWidth, baseHeight)) {
        waitForIdleOnce();
        hbaoBlurFinalTarget_ = GetRhiContext().CreateRenderTarget(
            RHI::RenderTargetDesc{
                .width = baseWidth,
                .height = baseHeight,
                .colorAttachmentCount = 1,
                .sampleCount = 1,
                .hasDepth = false
            }
        );
    }
}

void RenderContext::EnsureFallbackTextures() {
    if (hasFallbackBlackTexture_ && hasFallbackWhiteTexture_) {
        return;
    }
    if (!hasFallbackBlackTexture_) {
        RHI::Texture2DDesc blackDesc{};
        blackDesc.width = 1;
        blackDesc.height = 1;
        blackDesc.format = RHI::Format::R8G8B8A8_UNorm;
        blackDesc.linearFiltering = true;
        blackDesc.pixels = {0, 0, 0, 255};
        fallbackBlackTexture_ = GetRhiContext().CreateTexture2D(blackDesc);
        hasFallbackBlackTexture_ = fallbackBlackTexture_.graphic_handle_ptr != nullptr;
    }
    if (!hasFallbackWhiteTexture_) {
        RHI::Texture2DDesc whiteDesc{};
        whiteDesc.width = 1;
        whiteDesc.height = 1;
        whiteDesc.format = RHI::Format::R8G8B8A8_UNorm;
        whiteDesc.linearFiltering = true;
        whiteDesc.pixels = {255, 255, 255, 255};
        fallbackWhiteTexture_ = GetRhiContext().CreateTexture2D(whiteDesc);
        hasFallbackWhiteTexture_ = fallbackWhiteTexture_.graphic_handle_ptr != nullptr;
    }
}

std::optional<RenderContext::GpuMesh> RenderContext::CreateGpuMeshFromData(const Common::MeshData& mesh) {
    if (mesh.Vertices.empty() || mesh.Indices.empty()) {
        return std::nullopt;
    }
    EnsureBackend();
    auto vertexBuffer = GetRhiContext().CreateBuffer(RHI::BufferDesc{
        .type = RHI::BufferType::Vertex,
        .usage = RHI::BufferUsage::Static,
        .size = mesh.Vertices.size() * sizeof(Common::VertexP3N3),
        .initialData = mesh.Vertices.data()
    });
    auto indexBuffer = GetRhiContext().CreateBuffer(RHI::BufferDesc{
        .type = RHI::BufferType::Index,
        .usage = RHI::BufferUsage::Static,
        .size = mesh.Indices.size() * sizeof(RHI::u32),
        .initialData = mesh.Indices.data()
    });
    if (vertexBuffer == nullptr || indexBuffer == nullptr) {
        return std::nullopt;
    }
    return GpuMesh{
        .VertexBuffer = std::move(vertexBuffer),
        .IndexBuffer = std::move(indexBuffer),
        .IndexCount = static_cast<RHI::u32>(mesh.Indices.size()),
        .IndexType = RHI::IndexType::UInt32
    };
}

void RenderContext::InitializeGeometryBuffers() {
    if (!primitiveMeshCache_.empty()) {
        return;
    }
    static_cast<void>(GetOrCreatePrimitiveMesh(Enums::PartShape::Cube));
}

RenderContext::GpuMesh* RenderContext::GetOrCreatePrimitiveMesh(const Enums::PartShape shape) {
    if (const auto it = primitiveMeshCache_.find(shape); it != primitiveMeshCache_.end()) {
        return &it->second;
    }

    Common::MeshData mesh{};
    switch (shape) {
        case Enums::PartShape::Sphere:
            mesh = Common::Primitives::GenerateSphere();
            break;
        case Enums::PartShape::Cylinder:
            mesh = Common::Primitives::GenerateCylinder();
            break;
        case Enums::PartShape::Cone:
            mesh = Common::Primitives::GenerateCone();
            break;
        case Enums::PartShape::Cube:
        default:
            mesh = Common::Primitives::GenerateCube();
            break;
    }

    auto uploaded = CreateGpuMeshFromData(mesh);
    if (!uploaded.has_value()) {
        return nullptr;
    }
    auto [it, inserted] = primitiveMeshCache_.emplace(shape, GpuMesh{});
    if (inserted) {
        it->second = std::move(*uploaded);
    }
    return &it->second;
}

RenderContext::GpuMesh* RenderContext::GetOrCreateMeshPartMesh(const std::string& contentId, const bool smoothNormals) {
    LVS_BENCH_SCOPE("RenderContext::GetOrCreateMeshPartMesh");
    const auto resolvedPath = Context::ResolveContentPath(contentId);
    if (resolvedPath.empty()) {
        return nullptr;
    }

    const std::string key = resolvedPath.string() + (smoothNormals ? "|smooth" : "|flat");
    if (const auto it = meshPartCache_.find(key); it != meshPartCache_.end()) {
        return &it->second;
    }

    std::optional<Common::MeshData> loadedMesh;
    {
        LVS_BENCH_SCOPE("RenderContext::GetOrCreateMeshPartMesh::LoadMeshFromFile");
        loadedMesh = Common::LoadMeshFromFile(resolvedPath, smoothNormals);
    }
    if (!loadedMesh.has_value()) {
        return nullptr;
    }

    std::optional<GpuMesh> uploaded;
    {
        LVS_BENCH_SCOPE("RenderContext::GetOrCreateMeshPartMesh::CreateGpuMeshFromData");
        uploaded = CreateGpuMeshFromData(*loadedMesh);
    }
    if (!uploaded.has_value()) {
        return nullptr;
    }

    auto [it, inserted] = meshPartCache_.emplace(key, GpuMesh{});
    if (inserted) {
        it->second = std::move(*uploaded);
    }
    return &it->second;
}

const SceneData::MeshRef* RenderContext::GetOrCreateMeshRef(const std::string& key, const GpuMesh& mesh) {
    if (mesh.VertexBuffer == nullptr || mesh.IndexBuffer == nullptr || mesh.IndexCount == 0) {
        return nullptr;
    }
    if (const auto it = meshRefCache_.find(key); it != meshRefCache_.end()) {
        return it->second;
    }

    meshRefStorage_.push_back(SceneData::MeshRef{
        .VertexBuffer = mesh.VertexBuffer.get(),
        .IndexBuffer = mesh.IndexBuffer.get(),
        .IndexBufferType = mesh.IndexType,
        .IndexCount = mesh.IndexCount,
        .VertexOffset = 0,
        .IndexOffset = 0
    });
    const SceneData::MeshRef* ref = &meshRefStorage_.back();
    meshRefCache_.emplace(key, ref);
    return ref;
}

void RenderContext::TrimRetiredFrameResources() {
    constexpr std::size_t maxRetiredFrames = 3;
    while (retiredFrameResourceSets_.size() > maxRetiredFrames) {
        retiredFrameResourceSets_.pop_front();
    }
    while (retiredFrameUniformBuffers_.size() > maxRetiredFrames) {
        retiredFrameUniformBuffers_.pop_front();
    }
}

void RenderContext::UpdateSkyboxTexture() {
    if (place_ == nullptr) {
        return;
    }

    const auto snapshot = skyboxResolver_.Resolve(place_);
    skyboxTint_ = snapshot.Tint;
    const std::size_t resolvedKey = Context::BuildSkyboxSettingsKey(snapshot);
    if (skyboxSettingsKey_.has_value() && skyboxSettingsKey_.value() == resolvedKey && hasSkyboxCubemap_) {
        return;
    }

    try {
        auto cubemapDesc = Common::CubemapLoader::LoadFromSkyboxSettings(snapshot);
        auto texture = GetRhiContext().CreateTextureCube(cubemapDesc);
        if (texture.graphic_handle_ptr == nullptr) {
            return;
        }
        if (hasSkyboxCubemap_) {
            GetRhiContext().DestroyTexture(skyboxCubemap_);
        }
        skyboxCubemap_ = texture;
        hasSkyboxCubemap_ = true;
        skyboxSettingsKey_ = resolvedKey;
    } catch (const std::exception&) {
    }
}

void RenderContext::UpdateSurfaceAtlasTexture() {
    if (hasSurfaceAtlas_) {
        return;
    }
    const auto atlasPath = Utils::PathUtils::GetResourcePath("Surfaces/Surfaces.png");
    if (!Utils::FileIO::Exists(atlasPath)) {
        return;
    }

    try {
        const auto image = Utils::ImageIO::LoadRgba8(atlasPath);
        RHI::Texture2DDesc atlasDesc{};
        atlasDesc.width = image.Width;
        atlasDesc.height = image.Height;
        atlasDesc.format = RHI::Format::R8G8B8A8_UNorm;
        atlasDesc.linearFiltering = true;
        atlasDesc.generateMipmaps = requestedSurfaceMipmaps_;
        atlasDesc.pixels = image.Pixels;
        auto texture = GetRhiContext().CreateTexture2D(atlasDesc);
        if (texture.graphic_handle_ptr == nullptr) {
            return;
        }
        if (hasSurfaceAtlas_) {
            GetRhiContext().DestroyTexture(surfaceAtlas_);
        }
        surfaceAtlas_ = texture;
        hasSurfaceAtlas_ = true;
    } catch (const std::exception&) {
    }
}

void RenderContext::UpdateSurfaceNormalAtlasTexture() {
    if (hasSurfaceNormalAtlas_) {
        return;
    }
    const auto atlasPath = Utils::PathUtils::GetResourcePath("Surfaces/SurfacesNormals.png");
    if (!Utils::FileIO::Exists(atlasPath)) {
        return;
    }

    try {
        const auto image = Utils::ImageIO::LoadRgba8(atlasPath);
        RHI::Texture2DDesc atlasDesc{};
        atlasDesc.width = image.Width;
        atlasDesc.height = image.Height;
        atlasDesc.format = RHI::Format::R8G8B8A8_UNorm;
        atlasDesc.linearFiltering = true;
        atlasDesc.generateMipmaps = requestedSurfaceMipmaps_;
        atlasDesc.pixels = image.Pixels;
        auto texture = GetRhiContext().CreateTexture2D(atlasDesc);
        if (texture.graphic_handle_ptr == nullptr) {
            return;
        }
        if (hasSurfaceNormalAtlas_) {
            GetRhiContext().DestroyTexture(surfaceNormalAtlas_);
        }
        surfaceNormalAtlas_ = texture;
        hasSurfaceNormalAtlas_ = true;
    } catch (const std::exception&) {
    }
}

} // namespace Lvs::Engine::Rendering
