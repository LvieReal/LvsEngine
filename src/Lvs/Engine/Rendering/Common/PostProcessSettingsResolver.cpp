#include "Lvs/Engine/Rendering/Common/PostProcessSettingsResolver.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/Rendering/Common/EnvironmentLookup.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering::Common {

PostProcessSettingsSnapshot PostProcessSettingsResolver::Resolve(const std::shared_ptr<DataModel::Place>& place) const {
    PostProcessSettingsSnapshot snapshot{};
    const auto lighting = FindLightingService(place);
    if (lighting == nullptr) {
        return snapshot;
    }
    snapshot.GammaEnabled = lighting->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F;
    snapshot.DitheringEnabled = lighting->GetProperty("Dithering").toBool() ? 1.0F : 0.0F;
    snapshot.NeonBlur = std::max(
        kPostProcessMinNeonBlur,
        static_cast<float>(lighting->GetProperty("NeonBlur").toDouble())
    );
    return snapshot;
}

} // namespace Lvs::Engine::Rendering::Common
