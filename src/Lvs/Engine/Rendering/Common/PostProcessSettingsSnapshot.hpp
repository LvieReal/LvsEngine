#pragma once

#include "Lvs/Engine/Rendering/Common/PostProcessSettingsUtils.hpp"

namespace Lvs::Engine::Rendering::Common {

struct PostProcessSettingsSnapshot {
    float GammaEnabled{0.0F};
    float DitheringEnabled{0.0F};
    float NeonBlur{kPostProcessDefaultNeonBlur};
};

} // namespace Lvs::Engine::Rendering::Common
