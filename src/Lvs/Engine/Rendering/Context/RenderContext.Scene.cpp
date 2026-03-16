#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/DataModel/Services/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering {

#if 0
std::vector<SceneData::DrawPacket> RenderContext::BuildGeometryDraws(std::vector<Common::DrawInstanceData>& outInstances) {
    std::vector<SceneData::DrawPacket> draws;
    outInstances.clear();
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

    const auto pushOverlayDraw = [this, &draws, &computeSortDepth, &outInstances](const Common::OverlayPrimitive& overlay) {
        GpuMesh* gpuMesh = GetOrCreatePrimitiveMesh(overlay.Shape);
        if (gpuMesh == nullptr) {
            return;
        }
        const SceneData::MeshRef* meshRef = GetOrCreateMeshRef(
            "primitive:" + std::to_string(static_cast<int>(overlay.Shape)),
            *gpuMesh
        );
        if (meshRef == nullptr) {
            return;
        }
        Common::DrawInstanceData drawInstance{};
        drawInstance.Model = Context::ToFloatMat4ColumnMajor(overlay.Model);
        drawInstance.BaseColor = Context::ToVec4(overlay.Color, std::clamp(overlay.Alpha, 0.0F, 1.0F));
        drawInstance.Material = {
            std::clamp(overlay.Metalness, 0.0F, 1.0F),
            std::clamp(overlay.Roughness, 0.0F, 1.0F),
            std::max(0.0F, overlay.Emissive),
            overlay.IgnoreLighting ? 1.0F : 0.0F
        };
        drawInstance.SurfaceData0 = {0.0F, 0.0F, 0.0F, overlay.AlwaysOnTop ? 1.0F : 0.0F};
        drawInstance.SurfaceData1 = {0.0F, 0.0F, 0.0F, 0.0F};
        const auto rows = overlay.Model.Rows();
        const Math::Vector3 worldPosition{rows[0][3], rows[1][3], rows[2][3]};
        const float alpha = std::clamp(overlay.Alpha, 0.0F, 1.0F);
        const RHI::u32 baseInstance = static_cast<RHI::u32>(outInstances.size());
        outInstances.push_back(drawInstance);
        draws.push_back(SceneData::DrawPacket{
            .Mesh = meshRef,
            .BaseInstance = baseInstance,
            .InstanceCount = 1U,
            .CullMode = RHI::CullMode::Back,
            .Transparent = alpha < 1.0F || overlay.AlwaysOnTop,
            .AlwaysOnTop = overlay.AlwaysOnTop,
            .IgnoreLighting = overlay.IgnoreLighting,
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

                Common::DrawInstanceData drawInstance{};
                drawInstance.Model = Context::ToFloatMat4ColumnMajor(model);
                const float alpha = static_cast<float>(1.0 - std::clamp(transparency, 0.0, 1.0));
                drawInstance.BaseColor = Context::ToVec4(color, alpha);
                drawInstance.Material = {
                    static_cast<float>(std::clamp(part->GetProperty("Metalness").toDouble(), 0.0, 1.0)),
                    static_cast<float>(std::clamp(part->GetProperty("Roughness").toDouble(), 0.0, 1.0)),
                    static_cast<float>(std::max(0.0, part->GetProperty("Emissive").toDouble())),
                    0.0F
                };
                drawInstance.SurfaceData0 = {0.0F, 0.0F, 0.0F, 0.0F};
                drawInstance.SurfaceData1 = {0.0F, 0.0F, hasSurfaceAtlas_ ? 1.0F : 0.0F, 0.0F};
                if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(instance); partInstance != nullptr) {
                    drawInstance.SurfaceData0 = {
                        static_cast<float>(partInstance->GetProperty("TopSurface").value<Enums::PartSurfaceType>()),
                        static_cast<float>(partInstance->GetProperty("BottomSurface").value<Enums::PartSurfaceType>()),
                        static_cast<float>(partInstance->GetProperty("FrontSurface").value<Enums::PartSurfaceType>()),
                        static_cast<float>(partInstance->GetProperty("BackSurface").value<Enums::PartSurfaceType>())
                    };
                    drawInstance.SurfaceData1[0] = static_cast<float>(
                        partInstance->GetProperty("LeftSurface").value<Enums::PartSurfaceType>()
                    );
                    drawInstance.SurfaceData1[1] = static_cast<float>(
                        partInstance->GetProperty("RightSurface").value<Enums::PartSurfaceType>()
                    );
                }

                GpuMesh* gpuMesh = nullptr;
                if (const auto meshPart = std::dynamic_pointer_cast<Objects::MeshPart>(instance); meshPart != nullptr) {
                    const std::string contentId = meshPart->GetProperty("ContentId").toString().toStdString();
                    const bool smoothNormals = meshPart->GetProperty("SmoothNormals").toBool();
                    const auto resolvedPath = Context::ResolveContentPath(contentId);
                    if (resolvedPath.empty()) {
                        continue;
                    }
                    const std::string key = resolvedPath.string() + (smoothNormals ? "|smooth" : "|flat");
                    gpuMesh = GetOrCreateMeshPartMesh(
                        contentId,
                        smoothNormals
                    );
                    if (gpuMesh != nullptr) {
                        const SceneData::MeshRef* meshRef = GetOrCreateMeshRef("meshpart:" + key, *gpuMesh);
                        if (meshRef == nullptr) {
                            continue;
                        }
                        const bool alwaysOnTop = part->GetProperty("AlwaysOnTop").toBool();
                        const RHI::u32 baseInstance = static_cast<RHI::u32>(outInstances.size());
                        outInstances.push_back(drawInstance);
                        draws.push_back(SceneData::DrawPacket{
                            .Mesh = meshRef,
                            .BaseInstance = baseInstance,
                            .InstanceCount = 1U,
                            .CullMode = Context::ToRhiCullMode(part->GetProperty("CullMode").value<Enums::MeshCullMode>()),
                            .Transparent = alpha < 1.0F || alwaysOnTop,
                            .AlwaysOnTop = alwaysOnTop,
                            .IgnoreLighting = false,
                            .SortDepth = computeSortDepth(part->GetWorldPosition())
                        });
                    }
                    continue;
                } else {
                    Enums::PartShape shape = Enums::PartShape::Cube;
                    if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(instance); partInstance != nullptr) {
                        shape = partInstance->GetProperty("Shape").value<Enums::PartShape>();
                    }
                    gpuMesh = GetOrCreatePrimitiveMesh(shape);
                    if (gpuMesh == nullptr) {
                        continue;
                    }
                    const SceneData::MeshRef* meshRef = GetOrCreateMeshRef(
                        "primitive:" + std::to_string(static_cast<int>(shape)),
                        *gpuMesh
                    );
                    if (meshRef == nullptr) {
                        continue;
                    }

                    const bool alwaysOnTop = part->GetProperty("AlwaysOnTop").toBool();
                    const RHI::u32 baseInstance = static_cast<RHI::u32>(outInstances.size());
                    outInstances.push_back(drawInstance);
                    draws.push_back(SceneData::DrawPacket{
                        .Mesh = meshRef,
                        .BaseInstance = baseInstance,
                        .InstanceCount = 1U,
                        .CullMode = Context::ToRhiCullMode(part->GetProperty("CullMode").value<Enums::MeshCullMode>()),
                        .Transparent = alpha < 1.0F || alwaysOnTop,
                        .AlwaysOnTop = alwaysOnTop,
                        .IgnoreLighting = false,
                        .SortDepth = computeSortDepth(part->GetWorldPosition())
                    });
                    continue;
                }
            }
        }
    }

    for (const auto& overlay : overlayPrimitives_) {
        pushOverlayDraw(overlay);
    }
    return draws;
}
#endif

Common::CameraUniformData RenderContext::BuildCameraUniforms() {
    Common::CameraUniformData uniforms{};
    uniforms.View = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.Projection = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.CameraPosition = {0.0F, 0.0F, 0.0F, 1.0F};
    uniforms.LightDirection = {0.0F, -1.0F, 0.0F, 0.0F};
    uniforms.LightColorIntensity = {0.0F, 0.0F, 0.0F, 0.0F};
    uniforms.LightSpecular = {0.0F, 0.0F, 0.0F, 0.0F};
    uniforms.Ambient = {0.15F, 0.15F, 0.15F, 1.0F};
    uniforms.SkyTint = {1.0F, 1.0F, 1.0F, 1.0F};
    uniforms.RenderSettings = {1.0F, 0.0F, 1.0F, 0.0F};
    uniforms.ShadowMatrices[0] = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.ShadowMatrices[1] = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.ShadowMatrices[2] = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.ShadowCascadeSplits = {0.0F, 0.0F, 0.0F, 0.0F};
    uniforms.ShadowParams = {
        shadowSettings_.Bias,
        shadowSettings_.BlurAmount,
        static_cast<float>(shadowSettings_.TapCount),
        shadowSettings_.FadeWidth
    };
    uniforms.ShadowState = {0.0F, 0.0F, 0.0F, 0.0F};
    uniforms.CameraForward = {0.0F, 0.0F, -1.0F, 0.0F};

    if (place_ == nullptr) {
        return uniforms;
    }

    const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
    if (workspaceService == nullptr) {
        return uniforms;
    }

    Math::Vector3 directionalLightDirection = Math::Vector3{0.0, -1.0, 0.0};
    bool hasDirectionalLight = false;

    if (const auto lightingService = std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
        lightingService != nullptr) {
        const Math::Color3 ambientColor = lightingService->GetProperty("Ambient").value<Math::Color3>();
        uniforms.Ambient = Context::ToVec4(
            ambientColor,
            static_cast<float>(std::clamp(lightingService->GetProperty("AmbientStrength").toDouble(), 0.0, 1.0))
        );
        uniforms.RenderSettings = {
            lightingService->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F,
            lightingService->GetProperty("Dithering").toBool() ? 1.0F : 0.0F,
            lightingService->GetProperty("NeonEnabled").toBool() ? 1.0F : 0.0F,
            lightingService->GetProperty("InaccurateNeon").toBool() ? 1.0F : 0.0F
        };
        const auto shadingMode =
            lightingService->GetProperty("Shading").value<Enums::LightingComputationMode>();
        uniforms.ShadowState[3] = shadingMode == Enums::LightingComputationMode::PerVertex ? 1.0F : 0.0F;

        for (const auto& child : lightingService->GetChildren()) {
            const auto directional = std::dynamic_pointer_cast<Objects::DirectionalLight>(child);
            if (directional == nullptr || !directional->GetProperty("Enabled").toBool()) {
                continue;
            }
            const Math::Vector3 direction = directional->GetProperty("Direction").value<Math::Vector3>().Unit();
            const Math::Color3 color = directional->GetProperty("Color").value<Math::Color3>();
            const float intensity = static_cast<float>(std::max(0.0, directional->GetProperty("Intensity").toDouble()));
            uniforms.LightDirection = Context::ToVec4(direction, 0.0F);
            uniforms.LightColorIntensity = Context::ToVec4(color, intensity);
            uniforms.LightSpecular = {
                static_cast<float>(std::max(0.0, directional->GetProperty("SpecularStrength").toDouble())),
                static_cast<float>(std::max(0.0, directional->GetProperty("Shininess").toDouble())),
                0.0F,
                0.0F
            };
            directionalLightDirection = direction;
            hasDirectionalLight = true;
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

    uniforms.View = Context::ToFloatMat4ColumnMajor(camera->GetViewMatrix());
    const Math::Matrix4 projection = vkBackend_ != nullptr
                                         ? Context::ApplyVulkanProjectionFlip(camera->GetProjectionMatrix())
                                         : camera->GetProjectionMatrix();
    uniforms.Projection = Context::ToFloatMat4ColumnMajor(projection);
    const auto cframe = camera->GetProperty("CFrame").value<Math::CFrame>();
    uniforms.CameraPosition = Context::ToVec4(cframe.Position, 1.0F);
    uniforms.CameraForward = Context::ToVec4(cframe.LookVector(), 0.0F);

    if (shadowsEnabled_ && hasDirectionalLight) {
        const float aspect = surfaceHeight_ > 0U ? static_cast<float>(surfaceWidth_) / static_cast<float>(surfaceHeight_) : 1.0F;
        Common::ShadowCascadeComputation computation{};
        const bool ok = Common::ComputeShadowCascades(
            *camera,
            directionalLightDirection,
            aspect,
            shadowSettings_,
            shadowCascadeResolutions_,
            computation
        );
        if (ok) {
            shadowCascadeComputation_ = computation;
            for (int i = 0; i < Common::kMaxShadowCascades; ++i) {
                uniforms.ShadowMatrices[static_cast<std::size_t>(i)] =
                    Context::ToFloatMat4ColumnMajor(computation.Matrices[static_cast<std::size_t>(i)]);
            }
            uniforms.ShadowCascadeSplits = {
                computation.Split0,
                computation.Split1,
                computation.MaxDistance,
                static_cast<float>(shadowSettings_.CascadeCount)
            };
            uniforms.ShadowParams = {
                shadowSettings_.Bias,
                shadowSettings_.BlurAmount,
                static_cast<float>(shadowSettings_.TapCount),
                shadowSettings_.FadeWidth
            };
            uniforms.ShadowState[0] = 1.0F;
            uniforms.ShadowState[1] = shadowJitterScaleXY_;
            uniforms.ShadowState[2] = shadowJitterScaleXY_;
        } else {
            uniforms.ShadowState[0] = 0.0F;
        }
    } else {
        uniforms.ShadowState[0] = 0.0F;
    }
    return uniforms;
}

Common::SkyboxPushConstants RenderContext::BuildSkyboxPushConstants() const {
    Common::SkyboxPushConstants push{};
    push.ViewProjection = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    push.Tint = Context::ToVec4(skyboxTint_, 1.0F);

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
                                         ? Context::ApplyVulkanProjectionFlip(camera->GetProjectionMatrix())
                                         : camera->GetProjectionMatrix();
    const Math::Matrix4 viewProjection = projection * camera->GetViewMatrixNoTranslation();
    push.ViewProjection = Context::ToFloatMat4ColumnMajor(viewProjection);
    return push;
}

} // namespace Lvs::Engine::Rendering
