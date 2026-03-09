#pragma once

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
class Lighting;
}

namespace Lvs::Engine::Objects {
class Skybox;
}

namespace Lvs::Engine::Rendering::Common {

[[nodiscard]] std::shared_ptr<DataModel::Lighting> FindLightingService(const std::shared_ptr<DataModel::Place>& place);
[[nodiscard]] std::shared_ptr<Objects::Skybox> FindSkyboxInstance(const std::shared_ptr<DataModel::Lighting>& lighting);

} // namespace Lvs::Engine::Rendering::Common
