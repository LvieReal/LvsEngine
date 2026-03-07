#pragma once

#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"

namespace Lvs::Engine::Rendering::Common {

struct OverlayPrimitive {
    Math::Matrix4 Model{Math::Matrix4::Identity()};
    Enums::PartShape Shape{Enums::PartShape::Cube};
    Math::Color3 Color{};
    float Alpha{1.0F};
    float Metalness{0.0F};
    float Roughness{1.0F};
    float Emissive{1.0F};
    bool IgnoreLighting{false};
    bool AlwaysOnTop{true};
};

} // namespace Lvs::Engine::Rendering::Common
