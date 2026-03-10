#include "Lvs/Engine/Rendering/Common/ShadowCascadeUtils.hpp"

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Lvs::Engine::Rendering::Common {

namespace {

struct Vec4 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double w{0.0};
};

double Clamp(const double value, const double minValue, const double maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

Vec4 Multiply(const Math::Matrix4& matrix, const Vec4& vector) {
    const auto& m = matrix.Rows();
    return {
        m[0][0] * vector.x + m[0][1] * vector.y + m[0][2] * vector.z + m[0][3] * vector.w,
        m[1][0] * vector.x + m[1][1] * vector.y + m[1][2] * vector.z + m[1][3] * vector.w,
        m[2][0] * vector.x + m[2][1] * vector.y + m[2][2] * vector.z + m[2][3] * vector.w,
        m[3][0] * vector.x + m[3][1] * vector.y + m[3][2] * vector.z + m[3][3] * vector.w
    };
}

std::array<double, kMaxShadowCascades> ComputeCascadeSplits(
    const double nearPlane,
    const double farPlane,
    const int cascadeCount,
    const double lambda
) {
    std::array<double, kMaxShadowCascades> splits{farPlane, farPlane, farPlane};
    const double ratio = farPlane / std::max(nearPlane, 1e-4);
    const double clampedLambda = Clamp(lambda, 0.0, 1.0);
    for (int i = 1; i <= cascadeCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(cascadeCount);
        const double uniformSplit = nearPlane + ((farPlane - nearPlane) * t);
        const double logarithmicSplit = nearPlane * std::pow(ratio, t);
        const double split = (logarithmicSplit * clampedLambda) + (uniformSplit * (1.0 - clampedLambda));
        splits[static_cast<std::size_t>(i - 1)] = Clamp(split, nearPlane, farPlane);
    }
    splits[static_cast<std::size_t>(cascadeCount - 1)] = farPlane;
    return splits;
}

Math::Matrix4 BuildOrthographicZeroToOne(
    const double left,
    const double right,
    const double bottom,
    const double top,
    const double nearPlane,
    const double farPlane
) {
    const double width = std::max(1e-6, right - left);
    const double height = std::max(1e-6, top - bottom);
    const double depth = std::max(1e-6, farPlane - nearPlane);

    return Math::Matrix4({{
        {2.0 / width, 0.0, 0.0, -(right + left) / width},
        {0.0, 2.0 / height, 0.0, -(top + bottom) / height},
        {0.0, 0.0, -1.0 / depth, -nearPlane / depth},
        {0.0, 0.0, 0.0, 1.0}
    }});
}

Math::Matrix4 StabilizeProjection(
    const Math::Matrix4& projection,
    const Math::Matrix4& lightView,
    const double resolution
) {
    const auto shadowMatrix = projection * lightView;
    Vec4 shadowOrigin = Multiply(shadowMatrix, Vec4{0.0, 0.0, 0.0, 1.0});
    shadowOrigin.x *= (resolution * 0.5);
    shadowOrigin.y *= (resolution * 0.5);
    shadowOrigin.z *= (resolution * 0.5);

    const Vec4 rounded{
        std::round(shadowOrigin.x),
        std::round(shadowOrigin.y),
        std::round(shadowOrigin.z),
        std::round(shadowOrigin.w)
    };
    const Vec4 roundOffset{
        (rounded.x - shadowOrigin.x) * (2.0 / resolution),
        (rounded.y - shadowOrigin.y) * (2.0 / resolution),
        0.0,
        0.0
    };

    auto rows = projection.Rows();
    rows[0][3] += roundOffset.x;
    rows[1][3] += roundOffset.y;
    return Math::Matrix4(rows);
}

Math::Matrix4 ComputeCascadeLightViewProjection(
    const Objects::Camera& camera,
    const Math::Vector3& direction,
    const float cameraAspect,
    const double rangeNear,
    const double rangeFar,
    const std::uint32_t cascadeResolution,
    bool& success
) {
    success = false;
    if (rangeFar <= rangeNear + 1e-6) {
        return Math::Matrix4::Identity();
    }

    const auto cameraCFrame = camera.GetProperty("CFrame").value<Math::CFrame>();
    const Math::Vector3 camPos = cameraCFrame.Position;
    const Math::Vector3 camForward = cameraCFrame.LookVector().Unit();
    const Math::Vector3 camRight = cameraCFrame.RightVector().Unit();
    const Math::Vector3 camUp = cameraCFrame.UpVector().Unit();

    const double aspect = std::max(1e-6, static_cast<double>(cameraAspect));
    constexpr double DegToRad = 3.14159265358979323846 / 180.0;
    const double fovRadians = camera.GetProperty("FieldOfView").toDouble() * DegToRad;
    const double tanHalfFov = std::tan(fovRadians * 0.5);

    const double nearHeight = tanHalfFov * rangeNear;
    const double nearWidth = nearHeight * aspect;
    const double farHeight = tanHalfFov * rangeFar;
    const double farWidth = farHeight * aspect;

    const Math::Vector3 nearCenter = camPos + (camForward * rangeNear);
    const Math::Vector3 farCenter = camPos + (camForward * rangeFar);

    const std::array corners{
        nearCenter + (camUp * nearHeight) - (camRight * nearWidth),
        nearCenter + (camUp * nearHeight) + (camRight * nearWidth),
        nearCenter - (camUp * nearHeight) - (camRight * nearWidth),
        nearCenter - (camUp * nearHeight) + (camRight * nearWidth),
        farCenter + (camUp * farHeight) - (camRight * farWidth),
        farCenter + (camUp * farHeight) + (camRight * farWidth),
        farCenter - (camUp * farHeight) - (camRight * farWidth),
        farCenter - (camUp * farHeight) + (camRight * farWidth)
    };

    Math::Vector3 frustumCenter{0.0, 0.0, 0.0};
    for (const auto& corner : corners) {
        frustumCenter = frustumCenter + corner;
    }
    frustumCenter = frustumCenter * (1.0 / static_cast<double>(corners.size()));

    double radius = 0.0;
    for (const auto& corner : corners) {
        radius = std::max(radius, (corner - frustumCenter).Magnitude());
    }
    radius = std::max(radius, 1.0);

    const Math::Vector3 lightDir = direction.Unit();
    const Math::Vector3 eye = frustumCenter - (lightDir * radius);

    Math::Vector3 up{0.0, 1.0, 0.0};
    if (std::abs(lightDir.Dot(up)) > 0.98) {
        up = {0.0, 0.0, 1.0};
    }

    const auto lightFrame = Math::CFrame::LookAt(eye, frustumCenter, up);
    const auto lightView = lightFrame.Inverse().ToMatrix4();

    double minZ = std::numeric_limits<double>::infinity();
    double maxZ = -std::numeric_limits<double>::infinity();
    for (const auto& corner : corners) {
        const auto transformed = Multiply(lightView, Vec4{corner.x, corner.y, corner.z, 1.0});
        minZ = std::min(minZ, transformed.z);
        maxZ = std::max(maxZ, transformed.z);
    }

    constexpr double cascadeDepthMultiplier = 10.0;
    const double depthRadius = radius * cascadeDepthMultiplier;
    const double nearPlane = std::max(0.1, (-maxZ) - depthRadius);
    const double farPlane = std::max(nearPlane + 1.0, (-minZ) + depthRadius);

    auto projection = BuildOrthographicZeroToOne(-radius, radius, -radius, radius, nearPlane, farPlane);
    projection = StabilizeProjection(projection, lightView, static_cast<double>(std::max(1U, cascadeResolution)));
    success = true;
    return projection * lightView;
}

} // namespace

ShadowSettings NormalizeShadowSettings(const ShadowSettings& settings) {
    ShadowSettings normalized = settings;
    normalized.MapResolution = std::max<std::uint32_t>(128U, std::min<std::uint32_t>(8192U, normalized.MapResolution));
    normalized.CascadeResolutionScale = std::max(0.25F, std::min(1.0F, normalized.CascadeResolutionScale));
    normalized.CascadeSplitLambda = std::max(0.0F, std::min(1.0F, normalized.CascadeSplitLambda));
    normalized.TapCount = std::max(1, std::min(64, normalized.TapCount));
    normalized.BlurAmount = std::max(0.0F, std::min(12.0F, normalized.BlurAmount));
    normalized.CascadeCount = std::max(1, std::min(kMaxShadowCascades, normalized.CascadeCount));
    normalized.MaxDistance = std::max(1.0F, std::min(1024.0F, normalized.MaxDistance));
    // Bias and fade are currently engine-level defaults (no exposed user properties yet).
    normalized.Bias = 0.25F;
    normalized.FadeWidth = 0.25F;
    return normalized;
}

std::array<std::uint32_t, kMaxShadowCascades> ComputeCascadeResolutions(
    const std::uint32_t resolution,
    const float cascadeResolutionScale
) {
    const double clampedScale = std::clamp(static_cast<double>(cascadeResolutionScale), 0.25, 1.0);
    std::array<std::uint32_t, kMaxShadowCascades> cascadeResolutions{};
    for (int i = 0; i < kMaxShadowCascades; ++i) {
        const double scaledResolution = static_cast<double>(resolution) * std::pow(clampedScale, static_cast<double>(i));
        cascadeResolutions[static_cast<std::size_t>(i)] = std::max(
            128U,
            static_cast<std::uint32_t>(std::lround(std::max(128.0, scaledResolution)))
        );
    }
    return cascadeResolutions;
}

bool ComputeShadowCascades(
    const Objects::Camera& camera,
    const Math::Vector3& directionalLightDirection,
    const float cameraAspect,
    const ShadowSettings& settings,
    const std::array<std::uint32_t, kMaxShadowCascades>& cascadeResolutions,
    ShadowCascadeComputation& out
) {
    if (directionalLightDirection.MagnitudeSquared() <= 1e-8) {
        return false;
    }

    const double nearPlane = std::max(0.01, camera.GetProperty("NearPlane").toDouble());
    const double farPlane = std::max(nearPlane + 1.0, static_cast<double>(settings.MaxDistance));
    const auto splits = ComputeCascadeSplits(nearPlane, farPlane, settings.CascadeCount, settings.CascadeSplitLambda);

    double rangeNear = nearPlane;
    for (int i = 0; i < settings.CascadeCount; ++i) {
        bool success = true;
        const Math::Matrix4 matrix = ComputeCascadeLightViewProjection(
            camera,
            directionalLightDirection,
            cameraAspect,
            rangeNear,
            splits[static_cast<std::size_t>(i)],
            cascadeResolutions[static_cast<std::size_t>(i)],
            success
        );
        if (!success) {
            return false;
        }
        out.Matrices[static_cast<std::size_t>(i)] = matrix;
        rangeNear = splits[static_cast<std::size_t>(i)];
    }

    out.Split0 = static_cast<float>(settings.CascadeCount >= 2 ? splits[0] : farPlane);
    out.Split1 = static_cast<float>(settings.CascadeCount >= 3 ? splits[1] : farPlane);
    out.MaxDistance = static_cast<float>(farPlane);
    return true;
}

} // namespace Lvs::Engine::Rendering::Common
