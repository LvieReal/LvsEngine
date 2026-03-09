#pragma once

#include "Lvs/Engine/Rendering/Common/SkyboxSettingsSnapshot.hpp"
#include "Lvs/Engine/Rendering/RHI/Texture.hpp"

namespace Lvs::Engine::Rendering::Common {

class CubemapLoader {
public:
    [[nodiscard]] static RHI::CubemapDesc LoadFromSkyboxSettings(const SkyboxSettingsSnapshot& settings);
};

} // namespace Lvs::Engine::Rendering::Common
