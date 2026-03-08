#pragma once

#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

#include <array>
#include <filesystem>
#include <memory>

namespace Lvs::Engine::Objects {
class Skybox;
}

namespace Lvs::Engine::Rendering::Common {

struct SkyboxSettingsSnapshot {
    std::shared_ptr<Objects::Skybox> Source{};
    std::array<std::filesystem::path, 6> Faces{};
    Math::Color3 Tint{1.0, 1.0, 1.0};
    Enums::TextureFiltering Filtering{Enums::TextureFiltering::Linear};
    Enums::SkyboxTextureLayout TextureLayout{Enums::SkyboxTextureLayout::Individual};
    std::filesystem::path CrossTexture{};
    int ResolutionCap{0};
    bool Compression{false};
};

} // namespace Lvs::Engine::Rendering::Common
