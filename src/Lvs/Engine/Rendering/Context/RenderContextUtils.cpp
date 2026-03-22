#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"

#include "Lvs/Engine/Rendering/Renderer.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Lvs/Engine/Rendering/Backends/Vulkan/Utils/VulkanRenderUtils.hpp"

namespace Lvs::Engine::Rendering::Context {

std::array<float, 16> ToFloatMat4ColumnMajor(const Math::Matrix4& matrix) {
    const auto values = matrix.FlattenColumnMajor();
    std::array<float, 16> out{};
    for (std::size_t index = 0; index < out.size(); ++index) {
        out[index] = static_cast<float>(values[index]);
    }
    return out;
}

Math::Matrix4 ApplyVulkanProjectionFlip(const Math::Matrix4& projection) {
    auto rows = projection.Rows();
    rows[1][1] *= -1.0;
    return Math::Matrix4(rows);
}

// OpenGL uses NDC z in [-1, 1] while Vulkan uses [0, 1].
// If the incoming projection is authored for 0..1 clip-space, remap clip z for OpenGL as:
// z_gl = (2 * z_vk) - w
Math::Matrix4 ApplyOpenGLClipDepthRemap(const Math::Matrix4& clipZeroToOne) {
    auto rows = clipZeroToOne.Rows();
    for (std::size_t j = 0; j < 4; ++j) {
        rows[2][j] = (2.0 * rows[2][j]) - rows[3][j];
    }
    return Math::Matrix4(rows);
}

std::array<float, 4> ToVec4(const Math::Vector3& value, const float w) {
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z), w};
}

std::array<float, 4> ToVec4(const Math::Color3& value, const float w) {
    return {static_cast<float>(value.r), static_cast<float>(value.g), static_cast<float>(value.b), w};
}

namespace {

bool SupportsVulkan() {
    return Backends::Vulkan::Utils::SupportsVulkanRuntime();
}

std::size_t HashCombine(std::size_t seed, const std::size_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

} // namespace

RenderApi ResolveApi(const RenderApi preferred) {
    switch (preferred) {
        case RenderApi::Vulkan:
            return SupportsVulkan() ? RenderApi::Vulkan : RenderApi::OpenGL;
        case RenderApi::OpenGL:
            return RenderApi::OpenGL;
        case RenderApi::Auto:
        default:
            return SupportsVulkan() ? RenderApi::Vulkan : RenderApi::OpenGL;
    }
}

std::filesystem::path ResolveContentPath(const std::string& contentId) {
    if (contentId.empty()) {
        return {};
    }

    const auto directPath = Utils::PathUtils::ToOsPath(contentId);
    if (Utils::FileIO::Exists(directPath)) {
        return directPath;
    }

    const std::array<std::filesystem::path, 3> candidates{
        Utils::PathUtils::GetResourcePath(std::string("Meshes/") + contentId),
        Utils::PathUtils::GetSourcePath(std::string("src/Lvs/Engine/Content/Meshes/") + contentId),
        Utils::PathUtils::GetSourcePath(contentId)
    };

    for (const auto& candidate : candidates) {
        if (Utils::FileIO::Exists(candidate)) {
            return candidate;
        }
    }

    return {};
}

RHI::CullMode ToRhiCullMode(const Enums::MeshCullMode mode) {
    switch (mode) {
        case Enums::MeshCullMode::NoCull:
            return RHI::CullMode::None;
        case Enums::MeshCullMode::Front:
            return RHI::CullMode::Front;
        case Enums::MeshCullMode::Back:
        default:
            return RHI::CullMode::Back;
    }
}

std::size_t BuildSkyboxSettingsKey(const Common::SkyboxSettingsSnapshot& snapshot) {
    std::size_t key = 0;
    key = HashCombine(key, static_cast<std::size_t>(snapshot.TextureLayout));
    key = HashCombine(key, static_cast<std::size_t>(snapshot.Filtering));
    key = HashCombine(key, static_cast<std::size_t>(snapshot.ResolutionCap));
    key = HashCombine(key, snapshot.Compression ? 1U : 0U);
    key = HashCombine(key, std::hash<std::string>{}(snapshot.CrossTexture.string()));
    for (const auto& face : snapshot.Faces) {
        key = HashCombine(key, std::hash<std::string>{}(face.string()));
    }
    return key;
}

RHI::u32 ComputePostBlurLevels(const float blurAmount) {
    const float clampedBlur = std::max(0.0F, blurAmount);
    const RHI::u32 requested = static_cast<RHI::u32>(std::ceil(clampedBlur)) + 1U;
    return std::max<RHI::u32>(1U, std::min<RHI::u32>(SceneData::MaxPostBlurLevels, requested));
}

} // namespace Lvs::Engine::Rendering::Context
