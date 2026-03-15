#include "Lvs/Engine/Rendering/Common/EnvironmentLookup.hpp"

#include "Lvs/Engine/DataModel/Services/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"

namespace Lvs::Engine::Rendering::Common {

std::shared_ptr<DataModel::Lighting> FindLightingService(const std::shared_ptr<DataModel::Place>& place) {
    if (place == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<DataModel::Lighting>(place->FindService("Lighting"));
}

std::shared_ptr<Objects::Skybox> FindSkyboxInstance(const std::shared_ptr<DataModel::Lighting>& lighting) {
    if (lighting == nullptr) {
        return nullptr;
    }
    for (const auto& child : lighting->GetChildren()) {
        if (const auto sky = std::dynamic_pointer_cast<Objects::Skybox>(child); sky != nullptr) {
            return sky;
        }
    }
    return nullptr;
}

} // namespace Lvs::Engine::Rendering::Common
