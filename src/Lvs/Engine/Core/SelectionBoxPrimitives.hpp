#pragma once

#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Rendering/Vulkan/OverlayPrimitive.hpp"

#include <vector>

namespace Lvs::Engine::Core {

struct SelectionBoxStyle {
    Math::Color3 Color{0.0, 0.5, 1.0};
    float Alpha{1.0F};
    float Metalness{0.0F};
    float Roughness{1.0F};
    float Emissive{1.0F};
    bool IgnoreLighting{true};
    bool AlwaysOnTop{true};
    double Thickness{0.06};
};

void AppendSelectionBoxOutlinePrimitives(
    const Math::AABB& bounds,
    const SelectionBoxStyle& style,
    std::vector<Rendering::Vulkan::OverlayPrimitive>& out
);

} // namespace Lvs::Engine::Core
