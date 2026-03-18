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
