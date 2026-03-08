#pragma once

#include "Lvs/Engine/Rendering/Common/RenderSettingsSnapshot.hpp"

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Objects {
class DirectionalLight;
}

namespace Lvs::Engine::Rendering::Common {

class RenderSettingsResolver {
public:
    [[nodiscard]] RenderSettingsSnapshot Resolve(const std::shared_ptr<DataModel::Place>& place) const;
};

} // namespace Lvs::Engine::Rendering::Common

