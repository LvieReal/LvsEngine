#include "Lvs/Engine/Rendering/Common/RenderSettingsResolver.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/Rendering/Common/EnvironmentLookup.hpp"
#include "Lvs/Engine/Rendering/Common/ShadowCascadeUtils.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering::Common {

RenderSettingsSnapshot RenderSettingsResolver::Resolve(const std::shared_ptr<DataModel::Place>& place) const {
    RenderSettingsSnapshot snapshot{};
    const auto lighting = FindLightingService(place);
    if (lighting == nullptr) {
        return snapshot;
    }

    snapshot.AmbientColor = lighting->GetProperty("Ambient").value<Math::Color3>();
    snapshot.AmbientStrength = static_cast<float>(lighting->GetProperty("AmbientStrength").toDouble());
    snapshot.GammaCorrection = lighting->GetProperty("GammaCorrection").toBool();
    snapshot.Dithering = lighting->GetProperty("Dithering").toBool();
    snapshot.InaccurateNeon = lighting->GetProperty("InaccurateNeon").toBool();
    snapshot.ShadingMode = lighting->GetProperty("Shading").value<Enums::LightingComputationMode>();

    snapshot.Shadow.Enabled = lighting->GetProperty("ShadowsEnabled").toBool();
    snapshot.Shadow.BlurAmount = static_cast<float>(lighting->GetProperty("ShadowBlur").toDouble());
    snapshot.Shadow.TapCount = std::max(
        kShadowMinTapCount,
        std::min(kShadowMaxTapCount, lighting->GetProperty("DefaultShadowTapCount").toInt())
    );
    snapshot.Shadow.CascadeCount = std::max(
        1,
        std::min(kMaxShadowCascades, lighting->GetProperty("DefaultShadowCascadeCount").toInt())
    );
    snapshot.Shadow.MaxDistance = static_cast<float>(lighting->GetProperty("DefaultShadowMaxDistance").toDouble());
    snapshot.Shadow.MapResolution = static_cast<std::uint32_t>(std::max(
        static_cast<int>(kShadowMinResolution),
        lighting->GetProperty("DefaultShadowMapResolution").toInt()
    ));
    snapshot.Shadow.CascadeResolutionScale =
        static_cast<float>(lighting->GetProperty("DefaultShadowCascadeResolutionScale").toDouble());
    snapshot.Shadow.CascadeSplitLambda = static_cast<float>(lighting->GetProperty("DefaultShadowCascadeSplitLambda").toDouble());

    for (const auto& child : lighting->GetChildren()) {
        const auto light = std::dynamic_pointer_cast<Objects::DirectionalLight>(child);
        if (light == nullptr || !light->GetProperty("Enabled").toBool()) {
            continue;
        }
        snapshot.DirectionalLightEnabled = true;
        snapshot.DirectionalLightDirection = light->GetProperty("Direction").value<Math::Vector3>();
        snapshot.DirectionalLightColor = light->GetProperty("Color").value<Math::Color3>();
        snapshot.DirectionalLightIntensity = static_cast<float>(light->GetProperty("Intensity").toDouble());
        snapshot.DirectionalLightSpecularStrength = static_cast<float>(light->GetProperty("SpecularStrength").toDouble());
        snapshot.DirectionalLightShininess = static_cast<float>(light->GetProperty("Shininess").toDouble());
        break;
    }

    return snapshot;
}

} // namespace Lvs::Engine::Rendering::Common
