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
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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
    std::uint32_t version = 0;
    return vkEnumerateInstanceVersion(&version) == VK_SUCCESS && version > 0U;
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
        UpdateSkyboxTexture();
        SceneData scene{};
        scene.ClearColor = true;
        scene.ClearColorValue[0] = clearColor_[0];
        scene.ClearColorValue[1] = clearColor_[1];
        scene.ClearColorValue[2] = clearColor_[2];
        scene.ClearColorValue[3] = clearColor_[3];
        // Keep frame output source-of-truth on clear color until scene pass data is fully wired.
        scene.EnableShadows = false;
        scene.EnableSkybox = hasSkyboxCubemap_;
        scene.EnablePostProcess = false;
        scene.EnableGeometry = true;
        scene.ShadowTarget = SceneData::PassTarget{.RenderPass = nullptr, .Framebuffer = nullptr};
        scene.SkyboxTarget = scene.ShadowTarget;
        scene.PostProcessTarget = scene.ShadowTarget;
        scene.GeometryTarget = scene.ShadowTarget;

        frameMeshRefs_.clear();

        scene.ShadowDraw = {};
        scene.PostProcessDraw = {};
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
        std::array<RHI::ResourceBinding, 2> frameBindings{};
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
        frameResourceSet_ = GetRhiContext().CreateResourceSet(RHI::ResourceSetDesc{
            .bindings = frameBindings.data(),
            .bindingCount = frameBindingCount,
            .nativeHandleHint = nullptr
        });
        scene.GlobalResources = frameResourceSet_.get();

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
        if (hasSkyboxCubemap_ && (vkBackend_ != nullptr || glBackend_ != nullptr)) {
            GetRhiContext().DestroyTexture(skyboxCubemap_);
            skyboxCubemap_ = {};
            hasSkyboxCubemap_ = false;
        }
        frameResourceSet_.reset();
        frameUniformBuffer_.reset();
        retiredFrameResourceSets_.clear();
        retiredFrameUniformBuffers_.clear();
        primitiveMeshCache_.clear();
        meshPartCache_.clear();
        frameMeshRefs_.clear();
        skyboxSettingsKey_.reset();
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
            // Keep currently uploaded cubemap if loading the new one fails.
        }
    }

    [[nodiscard]] std::vector<SceneData::DrawPacket> BuildGeometryDraws() {
        std::vector<SceneData::DrawPacket> draws;
        const auto pushOverlayDraw = [this, &draws](const Common::OverlayPrimitive& overlay) {
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
            draws.push_back(SceneData::DrawPacket{
                .Mesh = meshRef,
                .PushConstants = push,
                .CullMode = RHI::CullMode::None
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
                    push.BaseColor = ToVec4(color, static_cast<float>(1.0 - std::clamp(transparency, 0.0, 1.0)));
                    push.Material = {
                        static_cast<float>(std::clamp(part->GetProperty("Roughness").toDouble(), 0.0, 1.0)),
                        static_cast<float>(std::clamp(part->GetProperty("Metalness").toDouble(), 0.0, 1.0)),
                        static_cast<float>(std::max(0.0, part->GetProperty("Emissive").toDouble())),
                        0.0F
                    };
                    push.SurfaceData0 = {0.0F, 0.0F, 0.0F, 0.0F};
                    push.SurfaceData1 = {0.0F, 0.0F, 0.0F, 0.0F};

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

                    draws.push_back(SceneData::DrawPacket{
                        .Mesh = meshRef,
                        .PushConstants = push,
                        .CullMode = ToRhiCullMode(part->GetProperty("CullMode").value<Enums::MeshCullMode>())
                    });
                }
            }
        }

        for (const auto& overlay : overlayPrimitives_) {
            pushOverlayDraw(overlay);
        }
        return draws;
    }

    [[nodiscard]] Common::CameraUniformData BuildCameraUniforms() const {
        Common::CameraUniformData uniforms{};
        uniforms.View = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.Projection = ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
        uniforms.CameraPosition = {0.0F, 0.0F, 0.0F, 1.0F};
        uniforms.LightDirection = {0.0F, -1.0F, 0.0F, 0.0F};
        uniforms.LightColorIntensity = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.LightSpecular = {0.0F, 0.0F, 0.0F, 0.0F};
        uniforms.Ambient = {0.15F, 0.15F, 0.15F, 1.0F};
        uniforms.SkyTint = {1.0F, 1.0F, 1.0F, 1.0F};
        uniforms.RenderSettings = {1.0F, 0.0F, 0.0F, 0.0F};
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
                lightingService->GetProperty("ShadowsEnabled").toBool() ? 1.0F : 0.0F,
                0.0F
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
    std::deque<std::unique_ptr<RHI::IBuffer>> retiredFrameUniformBuffers_{};
    std::deque<std::unique_ptr<RHI::IResourceSet>> retiredFrameResourceSets_{};
    Common::SkyboxSettingsResolver skyboxResolver_{};
    std::optional<std::size_t> skyboxSettingsKey_{};
    Math::Color3 skyboxTint_{1.0, 1.0, 1.0};
    RHI::Texture skyboxCubemap_{};
    bool hasSkyboxCubemap_{false};
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
