#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/QualitySettings.hpp"
#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace Lvs::Engine::Rendering {

void RenderContext::Render() {
    if (nativeWindowHandle_ == nullptr) {
        return;
    }
    EnsureBackend();
    RHI::u32 desiredMsaaSamples = 1U;
    if (place_ != nullptr) {
        if (const auto qualitySettings = std::dynamic_pointer_cast<DataModel::QualitySettings>(place_->FindService("QualitySettings"));
            qualitySettings != nullptr) {
            const int msaaValue = Enums::Metadata::IntFromVariant(qualitySettings->GetProperty("MSAA"));
            switch (msaaValue) {
                case 2:
                    desiredMsaaSamples = 2U;
                    break;
                case 4:
                    desiredMsaaSamples = 4U;
                    break;
                case 8:
                    desiredMsaaSamples = 8U;
                    break;
                default:
                    desiredMsaaSamples = 1U;
                    break;
            }
        }
    }
    if (desiredMsaaSamples != requestedMsaaSampleCount_) {
        requestedMsaaSampleCount_ = desiredMsaaSamples;
        effectiveMsaaSampleCount_ = desiredMsaaSamples;
        WaitForBackendIdle();
        geometryTarget_.reset();
    }
    EnsurePostProcessTargets();
    EnsureFallbackTextures();
    EnsureFallbackShadowTarget();
    EnsureShadowJitterTexture();
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
    Common::ShadowSettings desiredShadowSettings = shadowSettings_;
    if (place_ != nullptr) {
        if (const auto lightingService = std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
            lightingService != nullptr) {
            scene.NeonBlur = static_cast<float>(std::max(0.0, lightingService->GetProperty("NeonBlur").toDouble()));
            scene.EnableShadows = lightingService->GetProperty("ShadowsEnabled").toBool();
            bool hasDirectionalLight = false;
            for (const auto& child : lightingService->GetChildren()) {
                const auto directional = std::dynamic_pointer_cast<Objects::DirectionalLight>(child);
                if (directional != nullptr && directional->GetProperty("Enabled").toBool()) {
                    hasDirectionalLight = true;
                    break;
                }
            }
            scene.EnableShadows = scene.EnableShadows && hasDirectionalLight;
            desiredShadowSettings.BlurAmount = static_cast<float>(
                std::max(0.0, lightingService->GetProperty("ShadowBlur").toDouble())
            );
            desiredShadowSettings.TapCount = std::max(1, lightingService->GetProperty("DefaultShadowTapCount").toInt());
            desiredShadowSettings.CascadeCount =
                std::max(1, std::min(Common::kMaxShadowCascades, lightingService->GetProperty("DefaultShadowCascadeCount").toInt()));
            desiredShadowSettings.MaxDistance = static_cast<float>(
                std::max(1.0, lightingService->GetProperty("DefaultShadowMaxDistance").toDouble())
            );
            desiredShadowSettings.MapResolution = static_cast<std::uint32_t>(
                std::max(1, lightingService->GetProperty("DefaultShadowMapResolution").toInt())
            );
            desiredShadowSettings.CascadeResolutionScale = static_cast<float>(
                std::clamp(lightingService->GetProperty("DefaultShadowCascadeResolutionScale").toDouble(), 0.1, 1.0)
            );
            desiredShadowSettings.CascadeSplitLambda = static_cast<float>(
                std::clamp(lightingService->GetProperty("DefaultShadowCascadeSplitLambda").toDouble(), 0.0, 1.0)
            );
        }
    }
    shadowsEnabled_ = scene.EnableShadows;
    shadowSettings_ = Common::NormalizeShadowSettings(desiredShadowSettings);
    if (shadowsEnabled_) {
        EnsureShadowTargets(desiredShadowSettings);
        scene.ShadowCascadeCount = static_cast<RHI::u32>(std::clamp(shadowSettings_.CascadeCount, 0, Common::kMaxShadowCascades));
    } else {
        scene.ShadowCascadeCount = 0;
    }
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
        .Framebuffer = GetRhiContext().GetDefaultFramebufferHandle(),
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

    scene.ShadowCascadeTargets = {};
    if (shadowsEnabled_) {
        for (RHI::u32 cascade = 0; cascade < scene.ShadowCascadeCount && cascade < SceneData::MaxShadowCascades; ++cascade) {
            if (shadowTargets_[cascade] == nullptr) {
                continue;
            }
            scene.ShadowCascadeTargets[cascade] = SceneData::PassTarget{
                .RenderPass = shadowTargets_[cascade]->GetRenderPassHandle(),
                .Framebuffer = shadowTargets_[cascade]->GetFramebufferHandle(),
                .ColorAttachmentCount = shadowTargets_[cascade]->GetColorAttachmentCount(),
                .SampleCount = shadowTargets_[cascade]->GetSampleCount(),
                .Width = shadowTargets_[cascade]->GetWidth(),
                .Height = shadowTargets_[cascade]->GetHeight()
            };
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

    scene.ShadowCasters.clear();
    if (shadowsEnabled_ && scene.ShadowCascadeCount > 0U) {
        scene.ShadowCasters.reserve(scene.GeometryDraws.size());
        for (const auto& draw : scene.GeometryDraws) {
            if (draw.Mesh == nullptr) {
                continue;
            }
            if (draw.Transparent || draw.AlwaysOnTop) {
                continue;
            }
            if (draw.PushConstants.Material[3] > 0.5F) { // ignore lighting (gizmos/overlays)
                continue;
            }
            scene.ShadowCasters.push_back(draw);
        }
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
    if (frameShadowResourceSet_ != nullptr) {
        retiredFrameResourceSets_.push_back(std::move(frameShadowResourceSet_));
    }
    if (frameShadowUniformBuffer_ != nullptr) {
        retiredFrameUniformBuffers_.push_back(std::move(frameShadowUniformBuffer_));
    }
    frameUniformBuffer_ = GetRhiContext().CreateBuffer(RHI::BufferDesc{
        .type = RHI::BufferType::Uniform,
        .usage = RHI::BufferUsage::Dynamic,
        .size = sizeof(Common::CameraUniformData),
        .initialData = &cameraUniforms
    });
    scene.ShadowResources = nullptr;
    if (cameraUniforms.ShadowState[0] > 0.5F && scene.ShadowCascadeCount > 0U && !scene.ShadowCasters.empty()) {
        Common::ShadowUniformData shadowUniforms{};
        for (int i = 0; i < Common::kMaxShadowCascades; ++i) {
            Math::Matrix4 matrix = shadowCascadeComputation_.Matrices[static_cast<std::size_t>(i)];
            if (vkBackend_ == nullptr) {
                matrix = Context::ApplyOpenGLShadowDepthRemap(matrix);
            }
            shadowUniforms.LightViewProjection[static_cast<std::size_t>(i)] = Context::ToFloatMat4ColumnMajor(matrix);
        }
        frameShadowUniformBuffer_ = GetRhiContext().CreateBuffer(RHI::BufferDesc{
            .type = RHI::BufferType::Uniform,
            .usage = RHI::BufferUsage::Dynamic,
            .size = sizeof(Common::ShadowUniformData),
            .initialData = &shadowUniforms
        });
        const std::array<RHI::ResourceBinding, 1> shadowBindings{RHI::ResourceBinding{
            .slot = 0,
            .kind = RHI::ResourceBindingKind::UniformBuffer,
            .texture = {},
            .buffer = frameShadowUniformBuffer_.get()
        }};
        frameShadowResourceSet_ = GetRhiContext().CreateResourceSet(RHI::ResourceSetDesc{
            .bindings = shadowBindings.data(),
            .bindingCount = static_cast<RHI::u32>(shadowBindings.size()),
            .nativeHandleHint = nullptr
        });
        scene.ShadowResources = frameShadowResourceSet_.get();
    }

    std::array<RHI::ResourceBinding, 10> frameBindings{};
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
    const auto getCascadeTexture = [this, fallbackShadow](const RHI::u32 cascade) -> RHI::Texture {
        if (shadowsEnabled_ && cascade < shadowTargets_.size() && shadowTargets_[cascade] != nullptr) {
            return shadowTargets_[cascade]->GetDepthTexture();
        }
        return fallbackShadow;
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 3,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = getCascadeTexture(0),
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 4,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = getCascadeTexture(1),
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 5,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = getCascadeTexture(2),
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 6,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = (blurFinalTarget_ != nullptr ? blurFinalTarget_->GetColorTexture(0) : fallbackBlackTexture_),
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 7,
        .kind = RHI::ResourceBindingKind::Texture3D,
        .texture = shadowJitterTexture_,
        .buffer = nullptr
    };
    frameBindings[frameBindingCount++] = RHI::ResourceBinding{
        .slot = 8,
        .kind = RHI::ResourceBindingKind::Texture2D,
        .texture = fallbackBlackTexture_, // Normal atlas is currently optional.
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

} // namespace Lvs::Engine::Rendering
