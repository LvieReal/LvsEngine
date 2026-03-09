#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLApi.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanApi.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Common/MeshData.hpp"
#include "Lvs/Engine/Rendering/Common/MeshLoader.hpp"
#include "Lvs/Engine/Rendering/Common/Primitives.hpp"
#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxSettingsResolver.hpp"
#include "Lvs/Engine/Rendering/Common/CubemapLoader.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Rendering/Renderer.hpp"
#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/ImageIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Lvs::Engine::Rendering {

namespace {

std::array<float, 16> ToFloatMat4ColumnMajor(const Math::Matrix4& matrix) {
    const auto values = matrix.FlattenColumnMajor();
    std::array<float, 16> out{};
    for (std::size_t index = 0; index < out.size(); ++index) {
        out[index] = static_cast<float>(values[index]);
    }
    return out;
}

Math::Matrix4 ApplyVulkanProjectionFlip(const Math::Matrix4& projection) {
    auto rows = projection.Rows();
    rows[1][1] *= -1.0;
    return Math::Matrix4(rows);
}

std::array<float, 4> ToVec4(const Math::Vector3& value, const float w = 0.0F) {
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z), w};
}

std::array<float, 4> ToVec4(const Math::Color3& value, const float w = 1.0F) {
    return {static_cast<float>(value.r), static_cast<float>(value.g), static_cast<float>(value.b), w};
}

bool SupportsVulkan() {
    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion(&loaderVersion) != VK_SUCCESS) {
        return false;
    }
    const std::array<std::uint32_t, 4> candidates{
        VK_API_VERSION_1_3,
        VK_API_VERSION_1_2,
        VK_API_VERSION_1_1,
        VK_API_VERSION_1_0
    };
    for (const auto candidate : candidates) {
        if (VK_API_VERSION_MAJOR(loaderVersion) > VK_API_VERSION_MAJOR(candidate) ||
            (VK_API_VERSION_MAJOR(loaderVersion) == VK_API_VERSION_MAJOR(candidate) &&
             VK_API_VERSION_MINOR(loaderVersion) >= VK_API_VERSION_MINOR(candidate))) {
            return true;
        }
    }
    return false;
}

RenderApi ResolveApi(const RenderApi preferred) {
    switch (preferred) {
        case RenderApi::Vulkan:
            return SupportsVulkan() ? RenderApi::Vulkan : RenderApi::OpenGL;
        case RenderApi::OpenGL:
            return RenderApi::OpenGL;
        case RenderApi::Auto:
        default:
            return SupportsVulkan() ? RenderApi::Vulkan : RenderApi::OpenGL;
    }
}

std::filesystem::path ResolveContentPath(const std::string& contentId) {
    if (contentId.empty()) {
        return {};
    }

    const auto directPath = Utils::PathUtils::ToOsPath(contentId);
    if (Utils::FileIO::Exists(directPath)) {
        return directPath;
    }

    const std::array<std::filesystem::path, 3> candidates{
        Utils::PathUtils::GetResourcePath(std::string("Meshes/") + contentId),
        Utils::PathUtils::GetSourcePath(std::string("src/Lvs/Engine/Content/Meshes/") + contentId),
        Utils::PathUtils::GetSourcePath(contentId)
    };

    for (const auto& candidate : candidates) {
        if (Utils::FileIO::Exists(candidate)) {
            return candidate;
        }
    }

    return {};
}

RHI::CullMode ToRhiCullMode(const Enums::MeshCullMode mode) {
    switch (mode) {
        case Enums::MeshCullMode::NoCull:
            return RHI::CullMode::None;
        case Enums::MeshCullMode::Front:
            return RHI::CullMode::Front;
        case Enums::MeshCullMode::Back:
        default:
            return RHI::CullMode::Back;
    }
}

std::size_t HashCombine(std::size_t seed, const std::size_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

std::size_t BuildSkyboxSettingsKey(const Common::SkyboxSettingsSnapshot& snapshot) {
    std::size_t key = 0;
    key = HashCombine(key, static_cast<std::size_t>(snapshot.TextureLayout));
    key = HashCombine(key, static_cast<std::size_t>(snapshot.Filtering));
    key = HashCombine(key, static_cast<std::size_t>(snapshot.ResolutionCap));
    key = HashCombine(key, snapshot.Compression ? 1U : 0U);
    key = HashCombine(key, std::hash<std::string>{}(snapshot.CrossTexture.string()));
    for (const auto& face : snapshot.Faces) {
        key = HashCombine(key, std::hash<std::string>{}(face.string()));
    }
    return key;
}

RHI::u32 ComputePostBlurLevels(const float blurAmount) {
    const float clampedBlur = std::max(0.0F, blurAmount);
    const RHI::u32 requested = static_cast<RHI::u32>(std::ceil(clampedBlur)) + 1U;
    return std::max<RHI::u32>(1U, std::min<RHI::u32>(SceneData::MaxPostBlurLevels, requested));
}

} // namespace

class RenderContext final : public IRenderContext {
public:
    explicit RenderContext(const RenderApi preferredApi)
        : preferredApi_(preferredApi),
          activeApi_(ResolveApi(preferredApi_)) {}
    ~RenderContext() override {
        WaitForBackendIdle();
        ReleaseGpuResources();
        vkBackend_.reset();
        glBackend_.reset();
    }

    void Initialize(const RHI::u32 width, const RHI::u32 height) override {
        surfaceWidth_ = width;
        surfaceHeight_ = height;
        if (nativeWindowHandle_ == nullptr) {
            return;
        }
        EnsureBackend();
        if (vkBackend_ != nullptr) {
            vkBackend_->Initialize(width, height);
        } else if (glBackend_ != nullptr) {
            glBackend_->Initialize(width, height);
        } else {
            throw RenderingInitializationError("No render backend available");
        }
        InitializeGeometryBuffers();
    }

    void AttachToNativeWindow(void* nativeWindowHandle, const RHI::u32 width, const RHI::u32 height) override {
        WaitForBackendIdle();
        ReleaseGpuResources();
        nativeWindowHandle_ = nativeWindowHandle;
        vkApi_.NativeWindowHandle = nativeWindowHandle_;
        glApi_.NativeWindowHandle = nativeWindowHandle_;
        glApi_.ContextHandle = nullptr;
        vkBackend_.reset();
        glBackend_.reset();
        Initialize(width, height);
    }

    void Resize(const RHI::u32 width, const RHI::u32 height) override {
        surfaceWidth_ = width;
        surfaceHeight_ = height;
        if (nativeWindowHandle_ == nullptr) {
            return;
        }
        EnsureBackend();
        if (vkBackend_ != nullptr) {
            vkBackend_->Initialize(width, height);
        } else if (glBackend_ != nullptr) {
            glBackend_->Initialize(width, height);
        }
    }

    void SetClearColor(const float r, const float g, const float b, const float a) override {
        clearColor_[0] = r;
        clearColor_[1] = g;
        clearColor_[2] = b;
        clearColor_[3] = a;
    }

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place) override {
        place_ = place;
    }

    void Unbind() override {
        place_.reset();
        overlayPrimitives_.clear();
    }

    void SetOverlayPrimitives(std::vector<Common::OverlayPrimitive> primitives) override {
        overlayPrimitives_ = std::move(primitives);
    }

    void Render() override {
        if (nativeWindowHandle_ == nullptr) {
            return;
        }
        EnsureBackend();
        EnsurePostProcessTargets();
        EnsureFallbackTextures();
        UpdateSkyboxTexture();
        UpdateSurfaceAtlasTexture();
        SceneData scene{};
        scene.ClearColor = true;
        scene.ClearColorValue[0] = clearColor_[0];
        scene.ClearColorValue[1] = clearColor_[1];
        scene.ClearColorValue[2] = clearColor_[2];
        scene.ClearColorValue[3] = clearColor_[3];
        scene.EnableShadows = false;
        scene.EnableSkybox = hasSkyboxCubemap_;
        scene.EnablePostProcess = geometryTarget_ != nullptr && blurDownTargets_[0] != nullptr && blurFinalTarget_ != nullptr;
        scene.EnableGeometry = true;
        scene.NeonBlur = 2.0F;
        if (place_ != nullptr) {
            if (const auto lightingService = std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
                lightingService != nullptr) {
                scene.NeonBlur = static_cast<float>(std::max(0.0, lightingService->GetProperty("NeonBlur").toDouble()));
            }
        }
        scene.ShadowTarget = SceneData::PassTarget{
            .RenderPass = nullptr,
            .Framebuffer = nullptr,
            .ColorAttachmentCount = 1,
            .Width = surfaceWidth_,
            .Height = surfaceHeight_
        };
        scene.SkyboxTarget = scene.ShadowTarget;
        scene.GeometryTarget = scene.ShadowTarget;
        scene.PostBlurDownTarget = scene.ShadowTarget;
        scene.PostBlurUpTarget = scene.ShadowTarget;
        scene.PostBlurFinalTarget = scene.ShadowTarget;
        scene.PostProcessTarget = SceneData::PassTarget{
            .RenderPass = GetRhiContext().GetDefaultRenderPassHandle(),
            .Framebuffer = GetRhiContext().GetDefaultFramebufferHandle(),
            .ColorAttachmentCount = 1,
            .Width = surfaceWidth_,
            .Height = surfaceHeight_
        };
        if (geometryTarget_ != nullptr) {
            scene.SkyboxTarget = SceneData::PassTarget{
                .RenderPass = geometryTarget_->GetRenderPassHandle(),
                .Framebuffer = geometryTarget_->GetFramebufferHandle(),
                .ColorAttachmentCount = geometryTarget_->GetColorAttachmentCount(),
                .Width = geometryTarget_->GetWidth(),
                .Height = geometryTarget_->GetHeight()
            };
            scene.GeometryTarget = scene.SkyboxTarget;
        }
        RHI::u32 availableBlurLevels = 0U;
        for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
            if (blurDownTargets_[level] == nullptr || blurUpTargets_[level] == nullptr) {
                break;
            }
            ++availableBlurLevels;
        }
        const RHI::u32 postBlurLevels = scene.EnablePostProcess
                                            ? std::min(ComputePostBlurLevels(scene.NeonBlur), availableBlurLevels)
                                            : 0U;
        scene.PostBlurLevelCount = postBlurLevels;
        scene.EnablePostProcess = scene.EnablePostProcess && postBlurLevels > 0U && blurFinalTarget_ != nullptr;
        for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
            if (level < postBlurLevels && blurDownTargets_[level] != nullptr && blurUpTargets_[level] != nullptr) {
                scene.PostBlurDownLevelTargets[level] = SceneData::PassTarget{
                    .RenderPass = blurDownTargets_[level]->GetRenderPassHandle(),
                    .Framebuffer = blurDownTargets_[level]->GetFramebufferHandle(),
                    .ColorAttachmentCount = blurDownTargets_[level]->GetColorAttachmentCount(),
                    .Width = blurDownTargets_[level]->GetWidth(),
                    .Height = blurDownTargets_[level]->GetHeight()
                };
                scene.PostBlurUpLevelTargets[level] = SceneData::PassTarget{
                    .RenderPass = blurUpTargets_[level]->GetRenderPassHandle(),
                    .Framebuffer = blurUpTargets_[level]->GetFramebufferHandle(),
                    .ColorAttachmentCount = blurUpTargets_[level]->GetColorAttachmentCount(),
                    .Width = blurUpTargets_[level]->GetWidth(),
                    .Height = blurUpTargets_[level]->GetHeight()
                };
            } else {
                scene.PostBlurDownLevelTargets[level] = scene.ShadowTarget;
                scene.PostBlurUpLevelTargets[level] = scene.ShadowTarget;
            }
        }
        if (postBlurLevels > 0U) {
            scene.PostBlurDownTarget = scene.PostBlurDownLevelTargets[0];
            scene.PostBlurUpTarget = scene.PostBlurUpLevelTargets[0];
        }
        if (blurFinalTarget_ != nullptr) {
            scene.PostBlurFinalTarget = SceneData::PassTarget{
                .RenderPass = blurFinalTarget_->GetRenderPassHandle(),
                .Framebuffer = blurFinalTarget_->GetFramebufferHandle(),
                .ColorAttachmentCount = blurFinalTarget_->GetColorAttachmentCount(),
                .Width = blurFinalTarget_->GetWidth(),
                .Height = blurFinalTarget_->GetHeight()
            };
        }

        frameMeshRefs_.clear();

        scene.ShadowDraw = {};
        scene.SkyboxDraw = {};
        if (scene.EnableSkybox) {
            if (GpuMesh* skyboxMesh = GetOrCreatePrimitiveMesh(Enums::PartShape::Cube); skyboxMesh != nullptr) {
                if (const SceneData::MeshRef* skyboxMeshRef = PushFrameMeshRef(*skyboxMesh); skyboxMeshRef != nullptr) {
                    scene.SkyboxDraw = SceneData::DrawPacket{
                        .Mesh = skyboxMeshRef,
                        .PushConstants = {},
                        .CullMode = RHI::CullMode::Front
                    };
                } else {
                    scene.EnableSkybox = false;
                }
            } else {
                scene.EnableSkybox = false;
            }
        }
        scene.GeometryDraws = BuildGeometryDraws();
        if (!scene.GeometryDraws.empty()) {
            scene.GeometryDraw = scene.GeometryDraws.front();
        } else {
            scene.GeometryDraw = {};
        }

        ++postProcessFrameSeed_;
        const Common::CameraUniformData cameraUniforms = BuildCameraUniforms();
        scene.SkyboxPush = BuildSkyboxPushConstants();
        if (frameResourceSet_ != nullptr) {
            retiredFrameResourceSets_.push_back(std::move(frameResourceSet_));
        }
        if (frameUniformBuffer_ != nullptr) {
            retiredFrameUniformBuffers_.push_back(std::move(frameUniformBuffer_));
        }
        frameUniformBuffer_ = GetRhiContext().CreateBuffer(RHI::BufferDesc{
            .type = RHI::BufferType::Uniform,
            .usage = RHI::BufferUsage::Dynamic,
            .size = sizeof(Common::CameraUniformData),
            .initialData = &cameraUniforms
        });
        std::array<RHI::ResourceBinding, 4> frameBindings{};
        RHI::u32 frameBindingCount = 0;
        frameBindings[frameBindingCount++] = RHI::ResourceBinding{
            .slot = 0,
            .kind = RHI::ResourceBindingKind::UniformBuffer,
            .texture = {},
            .buffer = frameUniformBuffer_.get()
        };
        if (hasSkyboxCubemap_) {
            frameBindings[frameBindingCount++] = RHI::ResourceBinding{
                .slot = 1,
                .kind = RHI::ResourceBindingKind::TextureCube,
                .texture = skyboxCubemap_,
                .buffer = nullptr
            };
        }
        if (hasSurfaceAtlas_) {
            frameBindings[frameBindingCount++] = RHI::ResourceBinding{
                .slot = 2,
                .kind = RHI::ResourceBindingKind::Texture2D,
                .texture = surfaceAtlas_,
                .buffer = nullptr
            };
        }
        frameBindings[frameBindingCount++] = RHI::ResourceBinding{
            .slot = 6,
            .kind = RHI::ResourceBindingKind::Texture2D,
            .texture = (blurFinalTarget_ != nullptr ? blurFinalTarget_->GetColorTexture(0) : fallbackBlackTexture_),
            .buffer = nullptr
        };
        frameResourceSet_ = GetRhiContext().CreateResourceSet(RHI::ResourceSetDesc{
            .bindings = frameBindings.data(),
            .bindingCount = frameBindingCount,
            .nativeHandleHint = nullptr
        });
        scene.GlobalResources = frameResourceSet_.get();

        for (auto& set : postBlurDownLevelResourceSets_) {
            if (set != nullptr) {
                retiredFrameResourceSets_.push_back(std::move(set));
            }
        }
        for (auto& set : postBlurUpLevelResourceSets_) {
            if (set != nullptr) {
                retiredFrameResourceSets_.push_back(std::move(set));
            }
        }
        if (postBlurFinalResourceSet_ != nullptr) {
            retiredFrameResourceSets_.push_back(std::move(postBlurFinalResourceSet_));
        }
        if (postCompositeResourceSet_ != nullptr) {
            retiredFrameResourceSets_.push_back(std::move(postCompositeResourceSet_));
        }
        if (scene.EnablePostProcess) {
            scene.PostBlurDownLevelResources.fill(nullptr);
            scene.PostBlurUpLevelResources.fill(nullptr);
            const RHI::Texture sceneColor = geometryTarget_->GetColorTexture(0);
            const RHI::Texture sceneGlow = geometryTarget_->GetColorTexture(1);
            const RHI::u32 blurLevels = std::max<RHI::u32>(
                1U,
                std::min(scene.PostBlurLevelCount, SceneData::MaxPostBlurLevels)
            );
            for (RHI::u32 level = 0; level < blurLevels; ++level) {
                const RHI::Texture source = (level == 0U) ? sceneGlow : blurDownTargets_[level - 1]->GetColorTexture(0);
                const std::array<RHI::ResourceBinding, 1> bindings{RHI::ResourceBinding{
                    .slot = 1,
                    .kind = RHI::ResourceBindingKind::Texture2D,
                    .texture = source,
                    .buffer = nullptr
                }};
                postBlurDownLevelResourceSets_[level] = GetRhiContext().CreateResourceSet(
                    RHI::ResourceSetDesc{.bindings = bindings.data(), .bindingCount = 1}
                );
                scene.PostBlurDownLevelResources[level] = postBlurDownLevelResourceSets_[level].get();
            }

            if (blurLevels > 1U) {
                for (int level = static_cast<int>(blurLevels) - 2; level >= 0; --level) {
                    const RHI::Texture source = (level == static_cast<int>(blurLevels) - 2)
                                                    ? blurDownTargets_[blurLevels - 1]->GetColorTexture(0)
                                                    : blurUpTargets_[static_cast<RHI::u32>(level + 1)]->GetColorTexture(0);
                    const std::array<RHI::ResourceBinding, 1> bindings{RHI::ResourceBinding{
                        .slot = 1,
                        .kind = RHI::ResourceBindingKind::Texture2D,
                        .texture = source,
                        .buffer = nullptr
                    }};
                    postBlurUpLevelResourceSets_[static_cast<RHI::u32>(level)] = GetRhiContext().CreateResourceSet(
                        RHI::ResourceSetDesc{.bindings = bindings.data(), .bindingCount = 1}
                    );
                    scene.PostBlurUpLevelResources[static_cast<RHI::u32>(level)] =
                        postBlurUpLevelResourceSets_[static_cast<RHI::u32>(level)].get();
                }
            }

            const RHI::Texture finalBlurSource =
                (blurLevels > 1U) ? blurUpTargets_[0]->GetColorTexture(0) : blurDownTargets_[0]->GetColorTexture(0);
            const std::array<RHI::ResourceBinding, 1> finalBlurBindings{RHI::ResourceBinding{
                .slot = 1,
                .kind = RHI::ResourceBindingKind::Texture2D,
                .texture = finalBlurSource,
                .buffer = nullptr
            }};
            postBlurFinalResourceSet_ = GetRhiContext().CreateResourceSet(
                RHI::ResourceSetDesc{.bindings = finalBlurBindings.data(), .bindingCount = 1}
            );
            scene.PostBlurFinalResources = postBlurFinalResourceSet_.get();

            const std::array<RHI::ResourceBinding, 2> compositeBindings{
                RHI::ResourceBinding{
                    .slot = 1,
                    .kind = RHI::ResourceBindingKind::Texture2D,
                    .texture = sceneColor,
                    .buffer = nullptr
                },
                RHI::ResourceBinding{
                    .slot = 2,
                    .kind = RHI::ResourceBindingKind::Texture2D,
                    .texture = blurFinalTarget_->GetColorTexture(0),
                    .buffer = nullptr
                }
            };
            postCompositeResourceSet_ = GetRhiContext().CreateResourceSet(
                RHI::ResourceSetDesc{.bindings = compositeBindings.data(), .bindingCount = 2}
            );
            scene.PostBlurDownResources = scene.PostBlurDownLevelResources[0];
            scene.PostBlurUpResources = scene.PostBlurUpLevelResources[0];
            scene.PostCompositeResources = postCompositeResourceSet_.get();
            scene.PostProcessPush = Common::PostProcessPushConstants{
                .Settings = {cameraUniforms.RenderSettings[0], cameraUniforms.RenderSettings[1], cameraUniforms.RenderSettings[2], static_cast<float>(postProcessFrameSeed_)}
            };
        } else {
            scene.PostProcessPush = {};
            scene.PostBlurDownResources = nullptr;
            scene.PostBlurUpResources = nullptr;
            scene.PostBlurFinalResources = nullptr;
            scene.PostBlurDownLevelResources.fill(nullptr);
            scene.PostBlurUpLevelResources.fill(nullptr);
            scene.PostCompositeResources = nullptr;
        }

        static_cast<void>(place_);
        static_cast<void>(overlayPrimitives_);
        static_cast<void>(clearColor_);
        if (vkBackend_ != nullptr) {
            vkBackend_->Render(scene);
        } else if (glBackend_ != nullptr) {
            glBackend_->Render(scene);
        }
        TrimRetiredFrameResources();
    }

private:
    struct GpuMesh {
        std::unique_ptr<RHI::IBuffer> VertexBuffer{};
        std::unique_ptr<RHI::IBuffer> IndexBuffer{};
        RHI::u32 IndexCount{0};
        RHI::IndexType IndexType{RHI::IndexType::UInt32};
    };

    void WaitForBackendIdle() {
        if (vkBackend_ != nullptr) {
            vkBackend_->WaitIdle();
        }
        if (glBackend_ != nullptr) {
            glBackend_->WaitIdle();
        }
    }

    void ReleaseGpuResources() {
        if (hasSurfaceAtlas_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
            GetRhiContext().DestroyTexture(surfaceAtlas_);
            surfaceAtlas_ = {};
            hasSurfaceAtlas_ = false;
        }
        if (hasFallbackBlackTexture_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
            GetRhiContext().DestroyTexture(fallbackBlackTexture_);
            fallbackBlackTexture_ = {};
            hasFallbackBlackTexture_ = false;
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
        frameUniformBuffer_.reset();
        retiredFrameResourceSets_.clear();
        retiredFrameUniformBuffers_.clear();
        primitiveMeshCache_.clear();
        meshPartCache_.clear();
        frameMeshRefs_.clear();
        geometryTarget_.reset();
        for (auto& target : blurDownTargets_) {
            target.reset();
        }
        for (auto& target : blurUpTargets_) {
            target.reset();
        }
        blurFinalTarget_.reset();
        skyboxSettingsKey_.reset();
    }

    void EnsurePostProcessTargets() {
        if (surfaceWidth_ == 0U || surfaceHeight_ == 0U) {
            return;
        }
        const auto needsRecreate = [this](const std::unique_ptr<RHI::IRenderTarget>& target,
                                          const RHI::u32 width,
                                          const RHI::u32 height,
                                          const RHI::u32 colors) {
            return target == nullptr || target->GetWidth() != width || target->GetHeight() != height ||
                   target->GetColorAttachmentCount() != colors;
        };
        if (needsRecreate(geometryTarget_, surfaceWidth_, surfaceHeight_, 2U)) {
            geometryTarget_ = GetRhiContext().CreateRenderTarget(
                RHI::RenderTargetDesc{.width = surfaceWidth_, .height = surfaceHeight_, .colorAttachmentCount = 2, .hasDepth = true}
            );
        }

        RHI::u32 levelWidth = std::max<RHI::u32>(1U, surfaceWidth_ / 2U);
        RHI::u32 levelHeight = std::max<RHI::u32>(1U, surfaceHeight_ / 2U);
        for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
            if (needsRecreate(blurDownTargets_[level], levelWidth, levelHeight, 1U)) {
                blurDownTargets_[level] = GetRhiContext().CreateRenderTarget(
                    RHI::RenderTargetDesc{
                        .width = levelWidth,
                        .height = levelHeight,
                        .colorAttachmentCount = 1,
                        .hasDepth = false
                    }
                );
            }
            if (needsRecreate(blurUpTargets_[level], levelWidth, levelHeight, 1U)) {
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
        if (needsRecreate(blurFinalTarget_, surfaceWidth_, surfaceHeight_, 1U)) {
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

    void EnsureFallbackTextures() {
        if (hasFallbackBlackTexture_) {
            return;
        }
        RHI::Texture2DDesc blackDesc{};
        blackDesc.width = 1;
        blackDesc.height = 1;
        blackDesc.format = RHI::Format::R8G8B8A8_UNorm;
        blackDesc.linearFiltering = true;
        blackDesc.pixels = {0, 0, 0, 255};
        fallbackBlackTexture_ = GetRhiContext().CreateTexture2D(blackDesc);
        hasFallbackBlackTexture_ = fallbackBlackTexture_.graphic_handle_ptr != nullptr;
    }

    void EnsureBackend() {
        if (vkBackend_ != nullptr || glBackend_ != nullptr) {
            return;
        }

        activeApi_ = ResolveApi(preferredApi_);
        if (activeApi_ == RenderApi::Vulkan) {
            vkBackend_ = std::make_unique<Backends::Vulkan::VulkanContext>(vkApi_);
        } else {
            glBackend_ = std::make_unique<Backends::OpenGL::GLContext>(glApi_);
        }
    }

    RHI::IContext& GetRhiContext() {
        if (vkBackend_ != nullptr) {
            return *vkBackend_;
        }
        return *glBackend_;
    }

    [[nodiscard]] std::optional<GpuMesh> CreateGpuMeshFromData(const Common::MeshData& mesh) {
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

    void InitializeGeometryBuffers() {
        if (!primitiveMeshCache_.empty()) {
            return;
        }
        static_cast<void>(GetOrCreatePrimitiveMesh(Enums::PartShape::Cube));
    }

    GpuMesh* GetOrCreatePrimitiveMesh(const Enums::PartShape shape) {
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

    GpuMesh* GetOrCreateMeshPartMesh(const std::string& contentId, const bool smoothNormals) {
        const auto resolvedPath = ResolveContentPath(contentId);
        if (resolvedPath.empty()) {
            return nullptr;
        }

        const std::string key = resolvedPath.string() + (smoothNormals ? "|smooth" : "|flat");
        if (const auto it = meshPartCache_.find(key); it != meshPartCache_.end()) {
            return &it->second;
        }

        const auto loadedMesh = Common::LoadMeshFromFile(resolvedPath, smoothNormals);
        if (!loadedMesh.has_value()) {
            return nullptr;
        }
        auto uploaded = CreateGpuMeshFromData(*loadedMesh);
        if (!uploaded.has_value()) {
            return nullptr;
        }

        auto [it, inserted] = meshPartCache_.emplace(key, GpuMesh{});
        if (inserted) {
            it->second = std::move(*uploaded);
        }
        return &it->second;
    }

    SceneData::MeshRef* PushFrameMeshRef(const GpuMesh& mesh) {
        frameMeshRefs_.push_back(SceneData::MeshRef{
            .VertexBuffer = mesh.VertexBuffer.get(),
            .IndexBuffer = mesh.IndexBuffer.get(),
            .IndexBufferType = mesh.IndexType,
            .IndexCount = mesh.IndexCount,
            .VertexOffset = 0,
            .IndexOffset = 0
        });
        return &frameMeshRefs_.back();
    }

    void TrimRetiredFrameResources() {
        constexpr std::size_t maxRetiredFrames = 3;
        while (retiredFrameResourceSets_.size() > maxRetiredFrames) {
            retiredFrameResourceSets_.pop_front();
        }
        while (retiredFrameUniformBuffers_.size() > maxRetiredFrames) {
            retiredFrameUniformBuffers_.pop_front();
        }
    }

    void UpdateSkyboxTexture() {
        if (place_ == nullptr) {
            return;
        }

        const auto snapshot = skyboxResolver_.Resolve(place_);
        skyboxTint_ = snapshot.Tint;
        const std::size_t resolvedKey = BuildSkyboxSettingsKey(snapshot);
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

    void UpdateSurfaceAtlasTexture() {
        if (hasSurfaceAtlas_) {
            return;
        }
        const auto atlasPath = Utils::PathUtils::GetResourcePath("Surfaces/Surfaces2.png");
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

    [[nodiscard]] std::vector<SceneData::DrawPacket> BuildGeometryDraws() {
        std::vector<SceneData::DrawPacket> draws;
        bool hasCameraPosition = false;
        Math::Vector3 cameraPosition{};
        if (place_ != nullptr) {
            if (const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
                workspaceService != nullptr) {
                if (const auto camera =
                        workspaceService->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
                    camera != nullptr) {
                    cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
                    hasCameraPosition = true;
                }
            }
        }

        const auto computeSortDepth = [hasCameraPosition, cameraPosition](const Math::Vector3& worldPosition) {
            if (!hasCameraPosition) {
                return 0.0F;
            }
            const double dx = worldPosition.x - cameraPosition.x;
            const double dy = worldPosition.y - cameraPosition.y;
            const double dz = worldPosition.z - cameraPosition.z;
            return static_cast<float>(dx * dx + dy * dy + dz * dz);
        };

        const auto pushOverlayDraw = [this, &draws, &computeSortDepth](const Common::OverlayPrimitive& overlay) {
            GpuMesh* gpuMesh = GetOrCreatePrimitiveMesh(overlay.Shape);
            if (gpuMesh == nullptr) {
                return;
            }
            const SceneData::MeshRef* meshRef = PushFrameMeshRef(*gpuMesh);
            if (meshRef == nullptr) {
                return;
            }
            Common::DrawPushConstants push{};
            push.Model = ToFloatMat4ColumnMajor(overlay.Model);
            push.BaseColor = ToVec4(overlay.Color, std::clamp(overlay.Alpha, 0.0F, 1.0F));
            push.Material = {
                std::clamp(overlay.Roughness, 0.0F, 1.0F),
                std::clamp(overlay.Metalness, 0.0F, 1.0F),
                std::max(0.0F, overlay.Emissive),
                overlay.IgnoreLighting ? 1.0F : 0.0F
            };
            push.SurfaceData0 = {0.0F, 0.0F, 0.0F, overlay.AlwaysOnTop ? 1.0F : 0.0F};
            push.SurfaceData1 = {0.0F, 0.0F, 0.0F, 0.0F};
            const auto rows = overlay.Model.Rows();
            const Math::Vector3 worldPosition{rows[0][3], rows[1][3], rows[2][3]};
            const float alpha = std::clamp(overlay.Alpha, 0.0F, 1.0F);
            draws.push_back(SceneData::DrawPacket{
                .Mesh = meshRef,
                .PushConstants = push,
                .CullMode = RHI::CullMode::Back,
                .Transparent = alpha < 1.0F || overlay.AlwaysOnTop,
                .AlwaysOnTop = overlay.AlwaysOnTop,
                .SortDepth = computeSortDepth(worldPosition)
            });
        };

        if (place_ != nullptr) {
            const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
            if (workspaceService != nullptr) {
                const auto descendants = workspaceService->GetDescendants();
                draws.reserve(descendants.size() + overlayPrimitives_.size());
                for (const auto& instance : descendants) {
                    const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance);
                    if (part == nullptr) {
                        continue;
                    }
                    if (!part->GetProperty("Renders").toBool()) {
                        continue;
                    }

                    const double transparency = part->GetProperty("Transparency").toDouble();
                    if (transparency >= 1.0) {
                        continue;
                    }

                    const Math::Vector3 size = part->GetProperty("Size").value<Math::Vector3>();
                    if (size.x <= 0.0 || size.y <= 0.0 || size.z <= 0.0) {
                        continue;
                    }

                    const Math::Matrix4 model = part->GetWorldCFrame().ToMatrix4() * Math::Matrix4::Scale(size);
                    const Math::Color3 color = part->GetProperty("Color").value<Math::Color3>();

                    Common::DrawPushConstants push{};
                    push.Model = ToFloatMat4ColumnMajor(model);
                    const float alpha = static_cast<float>(1.0 - std::clamp(transparency, 0.0, 1.0));
                    push.BaseColor = ToVec4(color, alpha);
                    push.Material = {
                        static_cast<float>(std::clamp(part->GetProperty("Roughness").toDouble(), 0.0, 1.0)),
                        static_cast<float>(std::clamp(part->GetProperty("Metalness").toDouble(), 0.0, 1.0)),
                        static_cast<float>(std::max(0.0, part->GetProperty("Emissive").toDouble())),
                        0.0F
                    };
                    push.SurfaceData0 = {0.0F, 0.0F, 0.0F, 0.0F};
                    push.SurfaceData1 = {0.0F, 0.0F, hasSurfaceAtlas_ ? 1.0F : 0.0F, 0.0F};
                    if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(instance); partInstance != nullptr) {
                        push.SurfaceData0 = {
                            static_cast<float>(partInstance->GetProperty("TopSurface").value<Enums::PartSurfaceType>()),
                            static_cast<float>(partInstance->GetProperty("BottomSurface").value<Enums::PartSurfaceType>()),
                            static_cast<float>(partInstance->GetProperty("FrontSurface").value<Enums::PartSurfaceType>()),
                            static_cast<float>(partInstance->GetProperty("BackSurface").value<Enums::PartSurfaceType>())
                        };
                        push.SurfaceData1[0] = static_cast<float>(
                            partInstance->GetProperty("LeftSurface").value<Enums::PartSurfaceType>()
                        );
                        push.SurfaceData1[1] = static_cast<float>(
                            partInstance->GetProperty("RightSurface").value<Enums::PartSurfaceType>()
                        );
                    }

                    GpuMesh* gpuMesh = nullptr;
                    if (const auto meshPart = std::dynamic_pointer_cast<Objects::MeshPart>(instance); meshPart != nullptr) {
                        gpuMesh = GetOrCreateMeshPartMesh(
                            meshPart->GetProperty("ContentId").toString().toStdString(),
                            meshPart->GetProperty("SmoothNormals").toBool()
                        );
                    } else {
                        Enums::PartShape shape = Enums::PartShape::Cube;
                        if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(instance); partInstance != nullptr) {
                            shape = partInstance->GetProperty("Shape").value<Enums::PartShape>();
                        }
                        gpuMesh = GetOrCreatePrimitiveMesh(shape);
                    }
                    if (gpuMesh == nullptr) {
                        continue;
                    }
                    const SceneData::MeshRef* meshRef = PushFrameMeshRef(*gpuMesh);
                    if (meshRef == nullptr) {
                        continue;
                    }

                    const bool alwaysOnTop = part->GetProperty("AlwaysOnTop").toBool();
                    draws.push_back(SceneData::DrawPacket{
                        .Mesh = meshRef,
                        .PushConstants = push,
                        .CullMode = ToRhiCullMode(part->GetProperty("CullMode").value<Enums::MeshCullMode>()),
                        .Transparent = alpha < 1.0F || alwaysOnTop,
                        .AlwaysOnTop = alwaysOnTop,
                        .SortDepth = computeSortDepth(part->GetWorldPosition())
                    });
                }
            }
        }

        for (const auto& overlay : overlayPrimitives_) {
            pushOverlayDraw(overlay);
        }
        return draws;
    }

    [[nodiscard]] Common::CameraUniformData BuildCameraUniforms() {
        Common::CameraUniformData uniforms{};
        uniforms.View = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.Projection = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.CameraPosition = {0.0F, 0.0F, 0.0F, 1.0F};
        uniforms.LightDirection = {0.0F, -1.0F, 0.0F, 0.0F};
        uniforms.LightColorIntensity = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.LightSpecular = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.Ambient = {0.15F, 0.15F, 0.15F, 1.0F};
        uniforms.SkyTint = {1.0F, 1.0F, 1.0F, 1.0F};
        uniforms.RenderSettings = {1.0F, 0.0F, 1.0F, 0.0F};
        uniforms.ShadowMatrices[0] = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.ShadowMatrices[1] = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.ShadowMatrices[2] = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.ShadowCascadeSplits = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.ShadowParams = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.ShadowState = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.CameraForward = {0.0F, 0.0F, -1.0F, 0.0F};

        if (place_ == nullptr) {
            return uniforms;
        }

        const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
        if (workspaceService == nullptr) {
            return uniforms;
        }

        if (const auto lightingService = std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
            lightingService != nullptr) {
            const Math::Color3 ambientColor = lightingService->GetProperty("Ambient").value<Math::Color3>();
            uniforms.Ambient = ToVec4(
                ambientColor,
                static_cast<float>(std::clamp(lightingService->GetProperty("AmbientStrength").toDouble(), 0.0, 1.0))
            );
            uniforms.RenderSettings = {
                lightingService->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F,
                lightingService->GetProperty("Dithering").toBool() ? 1.0F : 0.0F,
                lightingService->GetProperty("NeonEnabled").toBool() ? 1.0F : 0.0F,
                lightingService->GetProperty("InaccurateNeon").toBool() ? 1.0F : 0.0F
            };

            for (const auto& child : lightingService->GetChildren()) {
                const auto directional = std::dynamic_pointer_cast<Objects::DirectionalLight>(child);
                if (directional == nullptr || !directional->GetProperty("Enabled").toBool()) {
                    continue;
                }
                const Math::Vector3 direction = directional->GetProperty("Direction").value<Math::Vector3>().Unit();
                const Math::Color3 color = directional->GetProperty("Color").value<Math::Color3>();
                const float intensity = static_cast<float>(std::max(0.0, directional->GetProperty("Intensity").toDouble()));
                uniforms.LightDirection = ToVec4(direction, 0.0F);
                uniforms.LightColorIntensity = ToVec4(color, intensity);
                uniforms.LightSpecular = {
                    static_cast<float>(std::max(0.0, directional->GetProperty("SpecularStrength").toDouble())),
                    static_cast<float>(std::max(0.0, directional->GetProperty("Shininess").toDouble())),
                    0.0F,
                    0.0F
                };
                break;
            }
        }

        const auto camera = workspaceService->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
        if (camera == nullptr) {
            return uniforms;
        }

        if (surfaceHeight_ > 0U) {
            camera->Resize(static_cast<double>(surfaceWidth_) / static_cast<double>(surfaceHeight_));
        }

        uniforms.View = ToFloatMat4ColumnMajor(camera->GetViewMatrix());
        const Math::Matrix4 projection = vkBackend_ != nullptr
                                             ? ApplyVulkanProjectionFlip(camera->GetProjectionMatrix())
                                             : camera->GetProjectionMatrix();
        uniforms.Projection = ToFloatMat4ColumnMajor(projection);
        const auto cframe = camera->GetProperty("CFrame").value<Math::CFrame>();
        uniforms.CameraPosition = ToVec4(cframe.Position, 1.0F);
        uniforms.CameraForward = ToVec4(cframe.LookVector(), 0.0F);
        return uniforms;
    }

    [[nodiscard]] Common::SkyboxPushConstants BuildSkyboxPushConstants() const {
        Common::SkyboxPushConstants push{};
        push.ViewProjection = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        push.Tint = ToVec4(skyboxTint_, 1.0F);

        if (place_ == nullptr) {
            return push;
        }
        const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
        if (workspaceService == nullptr) {
            return push;
        }
        const auto camera = workspaceService->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
        if (camera == nullptr) {
            return push;
        }

        if (surfaceHeight_ > 0U) {
            camera->Resize(static_cast<double>(surfaceWidth_) / static_cast<double>(surfaceHeight_));
        }

        const Math::Matrix4 projection = vkBackend_ != nullptr
                                             ? ApplyVulkanProjectionFlip(camera->GetProjectionMatrix())
                                             : camera->GetProjectionMatrix();
        const Math::Matrix4 viewProjection = projection * camera->GetViewMatrixNoTranslation();
        push.ViewProjection = ToFloatMat4ColumnMajor(viewProjection);
        return push;
    }

    RenderApi preferredApi_{RenderApi::Auto};
    RenderApi activeApi_{RenderApi::Auto};
    Backends::Vulkan::VulkanApi vkApi_{};
    Backends::OpenGL::GLApi glApi_{};
    std::unique_ptr<Backends::Vulkan::VulkanContext> vkBackend_{};
    std::unique_ptr<Backends::OpenGL::GLContext> glBackend_{};
    void* nativeWindowHandle_{nullptr};
    std::shared_ptr<DataModel::Place> place_{};
    std::vector<Common::OverlayPrimitive> overlayPrimitives_{};
    std::unordered_map<Enums::PartShape, GpuMesh> primitiveMeshCache_{};
    std::unordered_map<std::string, GpuMesh> meshPartCache_{};
    std::deque<SceneData::MeshRef> frameMeshRefs_{};
    std::unique_ptr<RHI::IBuffer> frameUniformBuffer_{};
    std::unique_ptr<RHI::IResourceSet> frameResourceSet_{};
    std::array<std::unique_ptr<RHI::IResourceSet>, SceneData::MaxPostBlurLevels> postBlurDownLevelResourceSets_{};
    std::array<std::unique_ptr<RHI::IResourceSet>, SceneData::MaxPostBlurLevels> postBlurUpLevelResourceSets_{};
    std::unique_ptr<RHI::IResourceSet> postBlurFinalResourceSet_{};
    std::unique_ptr<RHI::IResourceSet> postCompositeResourceSet_{};
    std::unique_ptr<RHI::IRenderTarget> geometryTarget_{};
    std::array<std::unique_ptr<RHI::IRenderTarget>, SceneData::MaxPostBlurLevels> blurDownTargets_{};
    std::array<std::unique_ptr<RHI::IRenderTarget>, SceneData::MaxPostBlurLevels> blurUpTargets_{};
    std::unique_ptr<RHI::IRenderTarget> blurFinalTarget_{};
    std::deque<std::unique_ptr<RHI::IBuffer>> retiredFrameUniformBuffers_{};
    std::deque<std::unique_ptr<RHI::IResourceSet>> retiredFrameResourceSets_{};
    Common::SkyboxSettingsResolver skyboxResolver_{};
    std::optional<std::size_t> skyboxSettingsKey_{};
    Math::Color3 skyboxTint_{1.0, 1.0, 1.0};
    RHI::Texture surfaceAtlas_{};
    bool hasSurfaceAtlas_{false};
    RHI::Texture skyboxCubemap_{};
    bool hasSkyboxCubemap_{false};
    RHI::Texture fallbackBlackTexture_{};
    bool hasFallbackBlackTexture_{false};
    std::uint32_t postProcessFrameSeed_{0};
    RHI::u32 surfaceWidth_{0};
    RHI::u32 surfaceHeight_{0};
    float clearColor_[4]{1.0F, 1.0F, 1.0F, 1.0F};
};

RenderApi ParseRenderApi(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "vulkan") {
        return RenderApi::Vulkan;
    }
    if (normalized == "opengl" || normalized == "gl") {
        return RenderApi::OpenGL;
    }
    return RenderApi::Auto;
}

std::unique_ptr<IRenderContext> CreateRenderContext(const RenderApi preferredApi) {
    return std::make_unique<RenderContext>(preferredApi);
}

} // namespace Lvs::Engine::Rendering
