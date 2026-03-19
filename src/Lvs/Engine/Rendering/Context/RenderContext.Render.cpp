#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Lighting.hpp"
#include "Lvs/Engine/DataModel/Services/QualitySettings.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <QMetaType>

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>

namespace Lvs::Engine::Rendering {

void RenderContext::Render() {
    LVS_BENCH_SCOPE("RenderContext::Render");
    if (nativeWindowHandle_ == nullptr) {
        return;
    }
    {
        LVS_BENCH_SCOPE("RenderContext::EnsureBackend");
        EnsureBackend();
    }
    if (refreshShadersRequested_) {
        refreshShadersRequested_ = false;
        WaitForBackendIdle();
        if (vkBackend_ != nullptr) {
            vkBackend_->RefreshShaders();
        }
        if (glBackend_ != nullptr) {
            glBackend_->RefreshShaders();
        }
    }
    RHI::u32 desiredMsaaSamples = 1U;
	    bool desiredSurfaceMipmaps = true;
	    if (place_ != nullptr) {
	        if (const auto qualitySettings = std::dynamic_pointer_cast<DataModel::QualitySettings>(place_->FindService("QualitySettings"));
	            qualitySettings != nullptr) {
	            const int msaaTypeId = QMetaType::fromType<Enums::MSAA>().id();
	            QVariant msaaValue = Enums::Metadata::CoerceVariant(msaaTypeId, qualitySettings->GetProperty("MSAA"));
	            if (!msaaValue.isValid()) {
	                msaaValue = QVariant::fromValue(Enums::MSAA::Off);
	            }
	            desiredMsaaSamples = static_cast<RHI::u32>(Enums::MsaaSampleCount(msaaValue.value<Enums::MSAA>()));

	            const int mipTypeId = QMetaType::fromType<Enums::SurfaceMipmapping>().id();
	            QVariant mipValue = Enums::Metadata::CoerceVariant(mipTypeId, qualitySettings->GetProperty("SurfaceMipmapping"));
	            if (!mipValue.isValid()) {
	                mipValue = QVariant::fromValue(Enums::SurfaceMipmapping::On);
	            }
	            desiredSurfaceMipmaps = Enums::IsSurfaceMipmappingEnabled(mipValue.value<Enums::SurfaceMipmapping>());
	        }
	    }
    if (desiredMsaaSamples != requestedMsaaSampleCount_) {
        requestedMsaaSampleCount_ = desiredMsaaSamples;
        effectiveMsaaSampleCount_ = desiredMsaaSamples;
        WaitForBackendIdle();
        geometryTarget_.reset();
    }
    if (desiredSurfaceMipmaps != requestedSurfaceMipmaps_) {
        requestedSurfaceMipmaps_ = desiredSurfaceMipmaps;
        WaitForBackendIdle();
        if (hasSurfaceAtlas_) {
            GetRhiContext().DestroyTexture(surfaceAtlas_);
            surfaceAtlas_ = {};
            hasSurfaceAtlas_ = false;
        }
    }
    EnsurePostProcessTargets();
    EnsureFallbackTextures();
    EnsureFallbackShadowTarget();
    EnsureShadowJitterTexture();
    UpdateSkyboxTexture();
    const bool hadSurfaceAtlas = hasSurfaceAtlas_;
    UpdateSurfaceAtlasTexture();
    if (hadSurfaceAtlas != hasSurfaceAtlas_) {
        MarkGeometryDataDirty();
    }
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

    std::shared_ptr<DataModel::Lighting> lightingService{};
    std::vector<std::shared_ptr<Objects::DirectionalLight>> enabledDirectionalLights{};
    enabledDirectionalLights.reserve(8);
    std::unordered_map<const Objects::DirectionalLight*, RHI::u32> directionalShadowIndex{};
    std::array<std::shared_ptr<Objects::DirectionalLight>, Common::kMaxDirectionalShadowMaps> shadowDirectionalLights{};
    std::array<Common::ShadowSettings, Common::kMaxDirectionalShadowMaps> requestedShadowSettings{};
    float fresnelAmount = 1.0F;

    if (place_ != nullptr) {
        lightingService = std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
        if (lightingService != nullptr) {
            scene.NeonBlur = static_cast<float>(std::max(0.0, lightingService->GetProperty("NeonBlur").toDouble()));
            fresnelAmount = static_cast<float>(std::clamp(lightingService->GetProperty("FresnelAmount").toDouble(), 0.0, 1.0));

            scene.DirectionalShadowCount = 0U;
            scene.DirectionalShadowCascadeCounts.fill(0U);
            shadowDirectionalLights.fill(nullptr);
            for (const auto& child : lightingService->GetChildren()) {
                const auto directional = std::dynamic_pointer_cast<Objects::DirectionalLight>(child);
                if (directional == nullptr || !directional->GetProperty("Enabled").toBool()) {
                    continue;
                }
                enabledDirectionalLights.push_back(directional);

                const bool wantsShadows = directional->GetProperty("ShadowEnabled").toBool();
                if (!wantsShadows || scene.DirectionalShadowCount >= Common::kMaxDirectionalShadowMaps) {
                    continue;
                }

                const RHI::u32 shadowIndex = scene.DirectionalShadowCount++;
                shadowDirectionalLights[shadowIndex] = directional;
                directionalShadowIndex[directional.get()] = shadowIndex;

                Common::ShadowSettings desired{};
                desired.BlurAmount = static_cast<float>(std::max(0.0, directional->GetProperty("ShadowBlur").toDouble()));
                desired.TapCount = std::max(1, directional->GetProperty("ShadowTapCount").toInt());
                desired.Bias = static_cast<float>(std::max(0.0, directional->GetProperty("ShadowBias").toDouble()));
                desired.SlopeBias = static_cast<float>(std::max(0.0, directional->GetProperty("ShadowSlopeBias").toDouble()));
                desired.CascadeCount = std::max(
                    1,
                    std::min(Common::kMaxShadowCascades, directional->GetProperty("ShadowCascadeCount").toInt())
                );
                desired.MaxDistance = static_cast<float>(std::max(1.0, directional->GetProperty("ShadowMaxDistance").toDouble()));
                desired.MapResolution = static_cast<std::uint32_t>(std::max(1, directional->GetProperty("ShadowMapResolution").toInt()));
                desired.CascadeResolutionScale = static_cast<float>(
                    std::clamp(directional->GetProperty("ShadowCascadeResolutionScale").toDouble(), 0.1, 1.0)
                );
                desired.CascadeSplitLambda = static_cast<float>(
                    std::clamp(directional->GetProperty("ShadowCascadeSplitLambda").toDouble(), 0.0, 1.0)
                );

                const Common::ShadowSettings normalized = Common::NormalizeShadowSettings(desired);
                requestedShadowSettings[shadowIndex] = normalized;
                EnsureDirectionalShadowTargets(shadowIndex, normalized);
                scene.DirectionalShadowCascadeCounts[shadowIndex] = static_cast<RHI::u32>(
                    std::clamp(normalized.CascadeCount, 0, Common::kMaxShadowCascades)
                );
            }
        }
    }
    scene.EnableShadows = scene.DirectionalShadowCount > 0U;
    scene.ShadowTarget = SceneData::PassTarget{
        .RenderPass = nullptr,
        .Framebuffer = nullptr,
        .ColorAttachmentCount = 1,
        .SampleCount = 1,
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
        .Framebuffer = nullptr,
        .ColorAttachmentCount = 1,
        .SampleCount = 1,
        .Width = surfaceWidth_,
        .Height = surfaceHeight_
    };
    if (geometryTarget_ != nullptr) {
        scene.SkyboxTarget = SceneData::PassTarget{
            .RenderPass = geometryTarget_->GetRenderPassHandle(),
            .Framebuffer = geometryTarget_->GetFramebufferHandle(),
            .ColorAttachmentCount = geometryTarget_->GetColorAttachmentCount(),
            .SampleCount = geometryTarget_->GetSampleCount(),
            .Width = geometryTarget_->GetWidth(),
            .Height = geometryTarget_->GetHeight()
        };
        scene.GeometryTarget = scene.SkyboxTarget;
    }

    scene.DirectionalShadowCascadeTargets = {};
    if (scene.EnableShadows) {
        const RHI::u32 shadowCount = std::min(scene.DirectionalShadowCount, SceneData::MaxDirectionalShadowMaps);
        for (RHI::u32 shadowIndex = 0; shadowIndex < shadowCount; ++shadowIndex) {
            const RHI::u32 cascadeCount = std::min(scene.DirectionalShadowCascadeCounts[shadowIndex], SceneData::MaxShadowCascades);
            for (RHI::u32 cascade = 0; cascade < cascadeCount; ++cascade) {
                if (directionalShadowTargets_[shadowIndex][cascade] == nullptr) {
                    continue;
                }
                const auto& rt = directionalShadowTargets_[shadowIndex][cascade];
                scene.DirectionalShadowCascadeTargets[shadowIndex][cascade] = SceneData::PassTarget{
                    .RenderPass = rt->GetRenderPassHandle(),
                    .Framebuffer = rt->GetFramebufferHandle(),
                    .ColorAttachmentCount = rt->GetColorAttachmentCount(),
                    .SampleCount = rt->GetSampleCount(),
                    .Width = rt->GetWidth(),
                    .Height = rt->GetHeight()
                };
            }
        }
    }
    RHI::u32 availableBlurLevels = 0U;
    for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
        if (blurDownTargets_[level] == nullptr || blurUpTargets_[level] == nullptr) {
            break;
        }
        ++availableBlurLevels;
    }
    const RHI::u32 postBlurLevels = scene.EnablePostProcess
                                        ? std::min(Context::ComputePostBlurLevels(scene.NeonBlur), availableBlurLevels)
                                        : 0U;
    scene.PostBlurLevelCount = postBlurLevels;
    scene.EnablePostProcess = scene.EnablePostProcess && postBlurLevels > 0U && blurFinalTarget_ != nullptr;
    for (RHI::u32 level = 0; level < SceneData::MaxPostBlurLevels; ++level) {
        if (level < postBlurLevels && blurDownTargets_[level] != nullptr && blurUpTargets_[level] != nullptr) {
            scene.PostBlurDownLevelTargets[level] = SceneData::PassTarget{
                .RenderPass = blurDownTargets_[level]->GetRenderPassHandle(),
                .Framebuffer = blurDownTargets_[level]->GetFramebufferHandle(),
                .ColorAttachmentCount = blurDownTargets_[level]->GetColorAttachmentCount(),
                .SampleCount = blurDownTargets_[level]->GetSampleCount(),
                .Width = blurDownTargets_[level]->GetWidth(),
                .Height = blurDownTargets_[level]->GetHeight()
            };
            scene.PostBlurUpLevelTargets[level] = SceneData::PassTarget{
                .RenderPass = blurUpTargets_[level]->GetRenderPassHandle(),
                .Framebuffer = blurUpTargets_[level]->GetFramebufferHandle(),
                .ColorAttachmentCount = blurUpTargets_[level]->GetColorAttachmentCount(),
                .SampleCount = blurUpTargets_[level]->GetSampleCount(),
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
            .SampleCount = blurFinalTarget_->GetSampleCount(),
            .Width = blurFinalTarget_->GetWidth(),
            .Height = blurFinalTarget_->GetHeight()
        };
    }

    scene.ShadowDraw = {};
    scene.SkyboxDraw = {};
    if (scene.EnableSkybox) {
        if (GpuMesh* skyboxMesh = GetOrCreatePrimitiveMesh(Enums::PartShape::Cube); skyboxMesh != nullptr) {
            const SceneData::MeshRef* skyboxMeshRef = GetOrCreateMeshRef(
                "primitive:" + std::to_string(static_cast<int>(Enums::PartShape::Cube)),
                *skyboxMesh
            );
            if (skyboxMeshRef != nullptr) {
                scene.SkyboxDraw = SceneData::DrawPacket{
                    .Mesh = skyboxMeshRef,
                    .BaseInstance = 0U,
                    .InstanceCount = 1U,
                    .CullMode = RHI::CullMode::Front
                };
            } else {
                scene.EnableSkybox = false;
            }
        } else {
            scene.EnableSkybox = false;
        }
    }
    {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws");
        // Split call vs. transfer so bench output can distinguish where time is spent.
        std::vector<SceneData::DrawPacket> geometryDraws{};
        {
            LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws(Call)");
            geometryDraws = BuildGeometryDraws();
        }
        {
            LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws(MoveToScene)");
            scene.GeometryDraws = std::move(geometryDraws);
        }
    }
    if (!scene.GeometryDraws.empty()) {
        scene.GeometryDraw = scene.GeometryDraws.front();
    } else {
        scene.GeometryDraw = {};
    }

    scene.ShadowCasters.clear();
    bool hasAnyShadowCascades = false;
    for (RHI::u32 i = 0; i < std::min(scene.DirectionalShadowCount, SceneData::MaxDirectionalShadowMaps); ++i) {
        hasAnyShadowCascades = hasAnyShadowCascades || scene.DirectionalShadowCascadeCounts[i] > 0U;
    }
    if (scene.EnableShadows && hasAnyShadowCascades) {
        scene.ShadowCasters.reserve(scene.GeometryDraws.size());
        for (const auto& draw : scene.GeometryDraws) {
            if (draw.Mesh == nullptr) {
                continue;
            }
            if (draw.Transparent || draw.AlwaysOnTop) {
                continue;
            }
            if (draw.IgnoreLighting) { // ignore lighting (gizmos/overlays)
                continue;
            }
            scene.ShadowCasters.push_back(draw);
        }
    }

    ++postProcessFrameSeed_;
    Common::CameraUniformData cameraUniforms{};
    {
        LVS_BENCH_SCOPE("RenderContext::BuildCameraUniforms");
        cameraUniforms = BuildCameraUniforms();
    }
    scene.SkyboxPush = BuildSkyboxPushConstants();
    if (frameResourceSet_ != nullptr) {
        retiredFrameResourceSets_.push_back(std::move(frameResourceSet_));
    }
    if (frameUniformBuffer_ != nullptr) {
        retiredFrameUniformBuffers_.push_back(std::move(frameUniformBuffer_));
    }
    for (auto& set : frameShadowResourceSets_) {
        if (set != nullptr) {
            retiredFrameResourceSets_.push_back(std::move(set));
        }
    }
    for (auto& buf : frameShadowUniformBuffers_) {
        if (buf != nullptr) {
            retiredFrameUniformBuffers_.push_back(std::move(buf));
        }
    }
    if (frameLightBuffer_ != nullptr) {
        retiredFrameUniformBuffers_.push_back(std::move(frameLightBuffer_));
    }
    {
        LVS_BENCH_SCOPE("RenderContext::CreateFrameUniformBuffer");
        frameUniformBuffer_ = GetRhiContext().CreateBuffer(RHI::BufferDesc{
        .type = RHI::BufferType::Uniform,
        .usage = RHI::BufferUsage::Dynamic,
        .size = sizeof(Common::CameraUniformData),
        .initialData = &cameraUniforms
        });
    }
    if (instanceBufferDirty_ || frameInstanceBuffer_ == nullptr) {
        if (frameInstanceBuffer_ != nullptr) {
            retiredFrameUniformBuffers_.push_back(std::move(frameInstanceBuffer_));
        }
        const Common::DrawInstanceData dummyInstance{};
        {
            LVS_BENCH_SCOPE("RenderContext::CreateInstanceBuffer");
            frameInstanceBuffer_ = GetRhiContext().CreateBuffer(RHI::BufferDesc{
            .type = RHI::BufferType::Storage,
            .usage = RHI::BufferUsage::Dynamic,
            .size = std::max<std::size_t>(static_cast<std::size_t>(1), cachedInstanceData_.size()) * sizeof(Common::DrawInstanceData),
            .initialData = cachedInstanceData_.empty() ? &dummyInstance : cachedInstanceData_.data()
            });
        }
        instanceBufferDirty_ = false;
    }
    scene.DirectionalShadowResources.fill(nullptr);

    // Resolve camera for shadow cascade computation.
    std::shared_ptr<Objects::Camera> camera{};
    if (place_ != nullptr) {
        if (const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
            workspaceService != nullptr) {
            camera = workspaceService->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
        }
    }
    const float aspect =
        surfaceHeight_ > 0U ? static_cast<float>(surfaceWidth_) / static_cast<float>(surfaceHeight_) : 1.0F;
    if (camera != nullptr && surfaceHeight_ > 0U) {
        camera->Resize(static_cast<double>(aspect));
    }

    // Precompute shadow cascades for each shadow-casting directional light.
    std::array<bool, Common::kMaxDirectionalShadowMaps> shadowCascadeOk{};
    for (RHI::u32 shadowIndex = 0; shadowIndex < std::min(scene.DirectionalShadowCount, SceneData::MaxDirectionalShadowMaps);
         ++shadowIndex) {
        shadowCascadeOk[shadowIndex] = false;
        const auto& light = shadowDirectionalLights[shadowIndex];
        if (light == nullptr || camera == nullptr || scene.DirectionalShadowCascadeCounts[shadowIndex] == 0U) {
            continue;
        }
        const Math::Vector3 direction = light->GetProperty("Direction").value<Math::Vector3>().Unit();
        Common::ShadowCascadeComputation computation{};
        const bool ok = Common::ComputeShadowCascades(
            *camera,
            direction,
            aspect,
            requestedShadowSettings[shadowIndex],
            directionalShadowCascadeResolutions_[shadowIndex],
            computation
        );
        if (!ok) {
            continue;
        }
        directionalShadowCascadeComputations_[shadowIndex] = computation;
        shadowCascadeOk[shadowIndex] = true;
    }

    // Build light buffer (all enabled directional lights; shadow data only for up to 2 shadowed directionals).
    Common::GpuLightBuffer lightBuffer{};
    std::uint32_t lightCount = 0U;
    std::uint32_t directionalCount = 0U;
    std::uint32_t shadowedDirectionalCount = static_cast<std::uint32_t>(scene.DirectionalShadowCount);

    for (const auto& directional : enabledDirectionalLights) {
        if (directional == nullptr) {
            continue;
        }
        if (lightCount >= Common::kMaxLights || directionalCount >= Common::kMaxDirectionalLights) {
            break;
        }

        Common::GpuLight base{};
        base.Type = static_cast<std::uint32_t>(Common::GpuLightType::Directional);
        base.Flags = Common::GpuLightFlagEnabled;
        base.DataIndex = directionalCount;
        base.ShadowIndex = 0xFFFFFFFFU;
        if (const auto it = directionalShadowIndex.find(directional.get()); it != directionalShadowIndex.end()) {
            base.ShadowIndex = it->second;
        }

        const Math::Color3 color = directional->GetProperty("Color").value<Math::Color3>();
        const float intensity = static_cast<float>(std::max(0.0, directional->GetProperty("Intensity").toDouble()));
        base.ColorIntensity = Context::ToVec4(color, intensity);
        base.Specular = {
            static_cast<float>(std::max(0.0, directional->GetProperty("SpecularStrength").toDouble())),
            static_cast<float>(std::max(0.0, directional->GetProperty("Shininess").toDouble())),
            fresnelAmount,
            0.0F
        };

        Common::GpuDirectionalLight dir{};
        const Math::Vector3 direction = directional->GetProperty("Direction").value<Math::Vector3>().Unit();
        dir.Direction = Context::ToVec4(direction, 0.0F);

        if (base.ShadowIndex != 0xFFFFFFFFU && base.ShadowIndex < scene.DirectionalShadowCount) {
            const RHI::u32 shadowIndex = base.ShadowIndex;
            const Common::ShadowSettings& settings = requestedShadowSettings[shadowIndex];
            const bool ok = shadowCascadeOk[shadowIndex];
            dir.ShadowState = {ok ? 1.0F : 0.0F, shadowJitterScaleXY_, shadowJitterScaleXY_, 0.0F};
            dir.ShadowCascadeSplits = {
                directionalShadowCascadeComputations_[shadowIndex].Split0,
                directionalShadowCascadeComputations_[shadowIndex].Split1,
                settings.MaxDistance,
                static_cast<float>(settings.CascadeCount)
            };
            dir.ShadowParams = {
                settings.Bias,
                settings.BlurAmount,
                static_cast<float>(settings.TapCount),
                settings.FadeWidth
            };
            dir.ShadowBiasParams = {settings.SlopeBias, 32.0F, 0.0F, 0.0F};

            for (int c = 0; c < Common::kMaxShadowCascades; ++c) {
                const auto matrix = directionalShadowCascadeComputations_[shadowIndex].Matrices[static_cast<std::size_t>(c)];
                dir.ShadowMatrices[static_cast<std::size_t>(c)] = Context::ToFloatMat4ColumnMajor(matrix);
                dir.ShadowInvMatrices[static_cast<std::size_t>(c)] = Context::ToFloatMat4ColumnMajor(matrix.Inverse());
            }
        } else {
            dir.ShadowState = {0.0F, 0.0F, 0.0F, 0.0F};
        }

        lightBuffer.Lights[lightCount] = base;
        lightBuffer.DirectionalLights[directionalCount] = dir;
        ++lightCount;
        ++directionalCount;
    }
    lightBuffer.Counts = {lightCount, directionalCount, shadowedDirectionalCount, 0U};

    frameLightBuffer_ = GetRhiContext().CreateBuffer(RHI::BufferDesc{
        .type = RHI::BufferType::Storage,
        .usage = RHI::BufferUsage::Dynamic,
        .size = sizeof(Common::GpuLightBuffer),
        .initialData = &lightBuffer
    });

    // Build shadow resources per shadow map (Directional shadow maps limited to 2).
    if (scene.EnableShadows && !scene.ShadowCasters.empty()) {
        for (RHI::u32 shadowIndex = 0; shadowIndex < std::min(scene.DirectionalShadowCount, SceneData::MaxDirectionalShadowMaps);
             ++shadowIndex) {
            if (!shadowCascadeOk[shadowIndex] || scene.DirectionalShadowCascadeCounts[shadowIndex] == 0U) {
                continue;
            }

            Common::ShadowUniformData shadowUniforms{};
            for (int i = 0; i < Common::kMaxShadowCascades; ++i) {
                Math::Matrix4 matrix = directionalShadowCascadeComputations_[shadowIndex].Matrices[static_cast<std::size_t>(i)];
                if (vkBackend_ == nullptr) {
                    matrix = Context::ApplyOpenGLShadowDepthRemap(matrix);
                }
                shadowUniforms.LightViewProjection[static_cast<std::size_t>(i)] = Context::ToFloatMat4ColumnMajor(matrix);
            }

            frameShadowUniformBuffers_[shadowIndex] = GetRhiContext().CreateBuffer(RHI::BufferDesc{
                .type = RHI::BufferType::Uniform,
                .usage = RHI::BufferUsage::Dynamic,
                .size = sizeof(Common::ShadowUniformData),
                .initialData = &shadowUniforms
            });

            const std::array<RHI::ResourceBinding, 2> shadowBindings{
                RHI::ResourceBinding{
                    .slot = 0,
                    .kind = RHI::ResourceBindingKind::UniformBuffer,
                    .texture = {},
                    .buffer = frameShadowUniformBuffers_[shadowIndex].get()
                },
                RHI::ResourceBinding{
                    .slot = 9,
                    .kind = RHI::ResourceBindingKind::StorageBuffer,
                    .texture = {},
                    .buffer = frameInstanceBuffer_.get()
                }
            };
            frameShadowResourceSets_[shadowIndex] = GetRhiContext().CreateResourceSet(RHI::ResourceSetDesc{
                .bindings = shadowBindings.data(),
                .bindingCount = static_cast<RHI::u32>(shadowBindings.size()),
                .nativeHandleHint = nullptr
            });
            scene.DirectionalShadowResources[shadowIndex] = frameShadowResourceSets_[shadowIndex].get();
        }
    }

    std::array<RHI::ResourceBinding, 20> frameBindings{};
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
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 2,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = hasSurfaceAtlas_ ? surfaceAtlas_ : fallbackBlackTexture_,
        .buffer = nullptr
    };
    const RHI::Texture fallbackShadow = (fallbackShadowTarget_ != nullptr) ? fallbackShadowTarget_->GetDepthTexture()
                                                                          : RHI::Texture{};
    const auto getDirectionalShadowTexture = [this, &scene, fallbackShadow](const RHI::u32 mapIndex) -> RHI::Texture {
        const RHI::u32 shadowIndex = mapIndex / Common::kDirectionalShadowMapCascadeCount;
        const RHI::u32 cascadeIndex = mapIndex % Common::kDirectionalShadowMapCascadeCount;
        if (!scene.EnableShadows || shadowIndex >= scene.DirectionalShadowCount) {
            return fallbackShadow;
        }
        if (cascadeIndex >= scene.DirectionalShadowCascadeCounts[shadowIndex]) {
            return fallbackShadow;
        }
        if (shadowIndex < directionalShadowTargets_.size() && cascadeIndex < SceneData::MaxShadowCascades &&
            directionalShadowTargets_[shadowIndex][cascadeIndex] != nullptr) {
            return directionalShadowTargets_[shadowIndex][cascadeIndex]->GetDepthTexture();
        }
        return fallbackShadow;
    };
    for (RHI::u32 i = 0; i < Common::kMaxDirectionalShadowMapTextures; ++i) {
        frameBindings[frameBindingCount++] = RHI::ResourceBinding{
            .slot = 3,
            .arrayElement = i,
            .kind = RHI::ResourceBindingKind::Texture2D,
            .texture = getDirectionalShadowTexture(i),
            .buffer = nullptr
        };
    }

    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 13,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = (blurFinalTarget_ != nullptr ? blurFinalTarget_->GetColorTexture(0) : fallbackBlackTexture_),
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 14,
        .kind = RHI::ResourceBindingKind::Texture3D,
        .texture = shadowJitterTexture_,
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 15,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = fallbackBlackTexture_, // Normal atlas is currently optional.
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 9,
        .kind = RHI::ResourceBindingKind::StorageBuffer,
        .texture = {},
        .buffer = frameInstanceBuffer_.get()
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 10,
        .kind = RHI::ResourceBindingKind::StorageBuffer,
        .texture = {},
        .buffer = frameLightBuffer_.get()
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
        {
            LVS_BENCH_SCOPE("RenderContext::BackendRender(Vulkan)");
            vkBackend_->Render(scene);
        }
    } else if (glBackend_ != nullptr) {
        {
            LVS_BENCH_SCOPE("RenderContext::BackendRender(OpenGL)");
            glBackend_->Render(scene);
        }
    }
    {
        LVS_BENCH_SCOPE("RenderContext::TrimRetiredFrameResources");
        TrimRetiredFrameResources();
    }
}

} // namespace Lvs::Engine::Rendering
