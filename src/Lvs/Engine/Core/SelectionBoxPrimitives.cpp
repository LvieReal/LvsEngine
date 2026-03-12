#include "Lvs/Engine/Core/SelectionBoxPrimitives.hpp"

#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace Lvs::Engine::Core {

namespace {

Rendering::Common::OverlayPrimitive BuildEdgePrimitive(
    const Math::Vector3& a,
    const Math::Vector3& b,
    const SelectionBoxStyle& style,
    double distanceFromCamera
) {
    const Math::Vector3 center = (a + b) * 0.5;
    const Math::Vector3 delta = b - a;
    const Math::Vector3 absDelta{std::abs(delta.x), std::abs(delta.y), std::abs(delta.z)};

    double thickness = std::max(0.001, style.Thickness);
    if (style.ScaleWithDistance) {
        thickness *= std::max(0.1, distanceFromCamera);
    }

    Math::Vector3 scale{thickness, thickness, thickness};
    // Extend each edge by thickness so adjacent edges overlap at corners.
    const double extendedLength = thickness;
    if (absDelta.x >= absDelta.y && absDelta.x >= absDelta.z) {
        scale.x = std::max(thickness, absDelta.x + extendedLength);
    } else if (absDelta.y >= absDelta.x && absDelta.y >= absDelta.z) {
        scale.y = std::max(thickness, absDelta.y + extendedLength);
    } else {
        scale.z = std::max(thickness, absDelta.z + extendedLength);
    }

    return Rendering::Common::OverlayPrimitive{
        .Model = Math::Matrix4::Translation(center) * Math::Matrix4::Scale(scale),
        .Shape = Enums::PartShape::Cube,
        .Color = style.Color,
        .Alpha = style.Alpha,
        .Metalness = style.Metalness,
        .Roughness = style.Roughness,
        .Emissive = style.Emissive,
        .IgnoreLighting = style.IgnoreLighting,
        .AlwaysOnTop = style.AlwaysOnTop
    };
}

} // namespace

void AppendSelectionBoxOutlinePrimitives(
    const Math::AABB& bounds,
    const SelectionBoxStyle& style,
    std::vector<Rendering::Common::OverlayPrimitive>& out,
    double distanceFromCamera
) {
    // Push the box slightly outside the target bounds to avoid z-fighting when depth-tested.
    double thickness = std::max(0.001, style.Thickness);
    if (style.ScaleWithDistance) {
        thickness *= std::max(0.1, distanceFromCamera);
    }
    const Math::Vector3 pad{(thickness * 0.5) + 0.001, (thickness * 0.5) + 0.001, (thickness * 0.5) + 0.001};
    const Math::Vector3 min = bounds.Min - pad;
    const Math::Vector3 max = bounds.Max + pad;

    const std::array<Math::Vector3, 8> corners{{
        {min.x, min.y, min.z}, // 0
        {max.x, min.y, min.z}, // 1
        {min.x, max.y, min.z}, // 2
        {max.x, max.y, min.z}, // 3
        {min.x, min.y, max.z}, // 4
        {max.x, min.y, max.z}, // 5
        {min.x, max.y, max.z}, // 6
        {max.x, max.y, max.z}, // 7
    }};

    constexpr std::array<std::array<int, 2>, 12> edges{{
        {{0, 1}}, {{2, 3}}, {{4, 5}}, {{6, 7}}, // X edges
        {{0, 2}}, {{1, 3}}, {{4, 6}}, {{5, 7}}, // Y edges
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}, // Z edges
    }};

    out.reserve(out.size() + edges.size());
    for (const auto& edge : edges) {
        out.push_back(BuildEdgePrimitive(corners[static_cast<std::size_t>(edge[0])], corners[static_cast<std::size_t>(edge[1])], style, distanceFromCamera));
    }
}

} // namespace Lvs::Engine::Core
