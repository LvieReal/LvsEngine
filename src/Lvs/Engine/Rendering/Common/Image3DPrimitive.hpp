#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

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
};

} // namespace Lvs::Engine::Rendering::Common

