#pragma once

#include "Lvs/Engine/Rendering/Common/SkyboxSettingsSnapshot.hpp"

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Rendering::Common {

class SkyboxSettingsResolver {
public:
    [[nodiscard]] SkyboxSettingsSnapshot Resolve(const std::shared_ptr<DataModel::Place>& place) const;
};

} // namespace Lvs::Engine::Rendering::Common
