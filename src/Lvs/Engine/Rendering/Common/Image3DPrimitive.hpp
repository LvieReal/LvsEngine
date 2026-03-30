#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <array>
#include <string>

namespace Lvs::Engine::Rendering::Common {

struct Image3DPrimitive {
    Math::Vector3 Position{};
    double Size{1.0};
    Math::Color3 Tint{1.0, 1.0, 1.0};
    float Alpha{1.0F};

    std::string ContentId{};
    int ResolutionCap{1024};

    bool FollowCamera{true};
    bool ConstantSize{true};
    double MaxDistance{1000.0};

    bool AlwaysOnTop{false};
    bool NegateMask{false};

    bool OutlineEnabled{false};
    Math::Color3 OutlineColor{0.0, 0.0, 0.0};
    double OutlineTransparency{0.0};
    double OutlineThickness{1.0};
};

} // namespace Lvs::Engine::Rendering::Common
