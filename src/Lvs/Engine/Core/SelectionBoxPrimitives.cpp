#include "Lvs/Engine/Core/SelectionBoxPrimitives.hpp"

#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
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

Math::Vector3 TransformPoint(const Math::Matrix4& matrix, const Math::Vector3& point) {
    const auto& m = matrix.Rows();
    const double x = m[0][0] * point.x + m[0][1] * point.y + m[0][2] * point.z + m[0][3];
    const double y = m[1][0] * point.x + m[1][1] * point.y + m[1][2] * point.z + m[1][3];
    const double z = m[2][0] * point.x + m[2][1] * point.y + m[2][2] * point.z + m[2][3];
    return {x, y, z};
}

Rendering::Common::OverlayPrimitive BuildEdgePrimitiveOriented(
    const Math::Vector3& a,
    const Math::Vector3& b,
    const SelectionBoxStyle& style,
    double distanceFromCamera
) {
    const Math::Vector3 center = (a + b) * 0.5;
    const Math::Vector3 delta = b - a;
    const double length = delta.Magnitude();

    double thickness = std::max(0.001, style.Thickness);
    if (style.ScaleWithDistance) {
        thickness *= std::max(0.1, distanceFromCamera);
    }

    if (length < 1e-8) {
        return Rendering::Common::OverlayPrimitive{
            .Model = Math::Matrix4::Translation(center) * Math::Matrix4::Scale({thickness, thickness, thickness}),
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

    const Math::Vector3 dir = delta.Unit();
    const Math::Vector3 helper = std::abs(dir.z) < 0.99 ? Math::Vector3{0.0, 0.0, 1.0} : Math::Vector3{0.0, 1.0, 0.0};
    const Math::Vector3 right = helper.Cross(dir).Unit();
    const Math::Vector3 back = right.Cross(dir).Unit();
    const Math::CFrame frame(center, right, dir, back);

    const double extendedLength = thickness;
    const Math::Vector3 scale{thickness, std::max(thickness, length + extendedLength), thickness};
    return Rendering::Common::OverlayPrimitive{
        .Model = frame.ToMatrix4() * Math::Matrix4::Scale(scale),
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

void AppendSelectionBoxOutlinePrimitivesRotated(
    const Math::CFrame& cframe,
    const Math::Vector3& size,
    const SelectionBoxStyle& style,
    std::vector<Rendering::Common::OverlayPrimitive>& out,
    double distanceFromCamera
) {
    double thickness = std::max(0.001, style.Thickness);
    if (style.ScaleWithDistance) {
        thickness *= std::max(0.1, distanceFromCamera);
    }
    const Math::Vector3 pad{(thickness * 0.5) + 0.001, (thickness * 0.5) + 0.001, (thickness * 0.5) + 0.001};
    const Math::Vector3 paddedSize = {size.x + pad.x * 2.0, size.y + pad.y * 2.0, size.z + pad.z * 2.0};
    const Math::Vector3 half = paddedSize * 0.5;

    const std::array<Math::Vector3, 8> localCorners{{
        {-half.x, -half.y, -half.z}, // 0
        {half.x, -half.y, -half.z},  // 1
        {-half.x, half.y, -half.z},  // 2
        {half.x, half.y, -half.z},   // 3
        {-half.x, -half.y, half.z},  // 4
        {half.x, -half.y, half.z},   // 5
        {-half.x, half.y, half.z},   // 6
        {half.x, half.y, half.z},    // 7
    }};

    const Math::Matrix4 world = cframe.ToMatrix4();
    std::array<Math::Vector3, 8> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        corners[i] = TransformPoint(world, localCorners[i]);
    }

    constexpr std::array<std::array<int, 2>, 12> edges{{
        {{0, 1}}, {{2, 3}}, {{4, 5}}, {{6, 7}}, // X edges
        {{0, 2}}, {{1, 3}}, {{4, 6}}, {{5, 7}}, // Y edges
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}, // Z edges
    }};

    out.reserve(out.size() + edges.size());
    for (const auto& edge : edges) {
        out.push_back(BuildEdgePrimitiveOriented(corners[static_cast<std::size_t>(edge[0])], corners[static_cast<std::size_t>(edge[1])], style, distanceFromCamera));
    }
}

} // namespace Lvs::Engine::Core
