#pragma once

#include "Lvs/Engine/Rendering/Common/PostProcessSettingsSnapshot.hpp"

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Rendering::Common {

class PostProcessSettingsResolver {
public:
    [[nodiscard]] PostProcessSettingsSnapshot Resolve(const std::shared_ptr<DataModel::Place>& place) const;
};

} // namespace Lvs::Engine::Rendering::Common

