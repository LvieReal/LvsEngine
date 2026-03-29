#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/DataModel/Services/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/DataModel/Objects/BasePart.hpp"
#include "Lvs/Engine/DataModel/Objects/Camera.hpp"
#include "Lvs/Engine/DataModel/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/DataModel/Objects/MeshPart.hpp"
#include "Lvs/Engine/DataModel/Objects/Part.hpp"
#include "Lvs/Engine/DataModel/Objects/PostEffects.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering {

Common::CameraUniformData RenderContext::BuildCameraUniforms() {
    Common::CameraUniformData uniforms{};
    uniforms.View = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.Projection = Context::ToFloatMat4ColumnMajor(Math::Matrix4::Identity());
    uniforms.CameraPosition = {0.0F, 0.0F, 0.0F, 1.0F};
    uniforms.Ambient = {0.15F, 0.15F, 0.15F, 1.0F};
    uniforms.SkyTint = {1.0F, 1.0F, 1.0F, 1.0F};
    uniforms.RenderSettings = {1.0F, 0.0F, 1.0F, 0.0F};
    uniforms.LightingSettings = {0.0F, 0.0F, 0.0F, 0.0F};
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
        uniforms.Ambient = Context::ToVec4(
            ambientColor,
            static_cast<float>(std::clamp(lightingService->GetProperty("AmbientStrength").toDouble(), 0.0, 1.0))
        );
        std::shared_ptr<DataModel::Objects::PostEffects> postEffects{};
        for (const auto& child : lightingService->GetChildren()) {
            postEffects = std::dynamic_pointer_cast<DataModel::Objects::PostEffects>(child);
            if (postEffects != nullptr) {
                break;
            }
        }

        if (postEffects != nullptr) {
            uniforms.RenderSettings = {
                postEffects->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F,
                postEffects->GetProperty("Dithering").toBool() ? 1.0F : 0.0F,
                postEffects->GetProperty("NeonEnabled").toBool() ? 1.0F : 0.0F,
                postEffects->GetProperty("InaccurateNeon").toBool() ? 1.0F : 0.0F
            };
        } else {
            uniforms.RenderSettings = {
                lightingService->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F,
                lightingService->GetProperty("Dithering").toBool() ? 1.0F : 0.0F,
                lightingService->GetProperty("NeonEnabled").toBool() ? 1.0F : 0.0F,
                lightingService->GetProperty("InaccurateNeon").toBool() ? 1.0F : 0.0F
            };
        }
        const auto shadingMode =
            lightingService->GetProperty("Shading").value<Enums::LightingComputationMode>();
        uniforms.LightingSettings[0] = shadingMode == Enums::LightingComputationMode::PerVertex ? 1.0F : 0.0F;
    }

    const auto cameraVar = workspaceService->GetProperty("CurrentCamera");
    std::shared_ptr<DataModel::Objects::Camera> camera{};
    if (cameraVar.Is<Core::Variant::InstanceRef>()) {
        if (const auto locked = cameraVar.Get<Core::Variant::InstanceRef>().lock()) {
            camera = std::dynamic_pointer_cast<DataModel::Objects::Camera>(locked);
        }
    }
    if (camera == nullptr) {
        return uniforms;
    }

    if (surfaceHeight_ > 0U) {
        camera->Resize(static_cast<double>(surfaceWidth_) / static_cast<double>(surfaceHeight_));
    }

    uniforms.View = Context::ToFloatMat4ColumnMajor(camera->GetViewMatrix());
    Math::Matrix4 projection = camera->GetProjectionMatrix();
    if (vkBackend_ != nullptr) {
        projection = Context::ApplyVulkanProjectionFlip(projection);
    } else {
        projection = Context::ApplyOpenGLClipDepthRemap(projection);
    }
    uniforms.Projection = Context::ToFloatMat4ColumnMajor(projection);
    const auto cframe = camera->GetProperty("CFrame").value<Math::CFrame>();
    uniforms.CameraPosition = Context::ToVec4(cframe.Position, 1.0F);
    uniforms.CameraForward = Context::ToVec4(cframe.LookVector(), 0.0F);
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
    const auto cameraVar = workspaceService->GetProperty("CurrentCamera");
    std::shared_ptr<DataModel::Objects::Camera> camera{};
    if (cameraVar.Is<Core::Variant::InstanceRef>()) {
        if (const auto locked = cameraVar.Get<Core::Variant::InstanceRef>().lock()) {
            camera = std::dynamic_pointer_cast<DataModel::Objects::Camera>(locked);
        }
    }
    if (camera == nullptr) {
        return push;
    }

    if (surfaceHeight_ > 0U) {
        camera->Resize(static_cast<double>(surfaceWidth_) / static_cast<double>(surfaceHeight_));
    }

    Math::Matrix4 projection = camera->GetProjectionMatrix();
    if (vkBackend_ != nullptr) {
        projection = Context::ApplyVulkanProjectionFlip(projection);
    } else {
        projection = Context::ApplyOpenGLClipDepthRemap(projection);
    }
    const Math::Matrix4 viewProjection = projection * camera->GetViewMatrixNoTranslation();
    push.ViewProjection = Context::ToFloatMat4ColumnMajor(viewProjection);
    return push;
}

} // namespace Lvs::Engine::Rendering
