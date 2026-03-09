#include "Lvs/Engine/Rendering/Common/CubemapLoader.hpp"

#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/ImageIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>

namespace Lvs::Engine::Rendering::Common {

namespace {

std::filesystem::path ResolvePath(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    const std::array<std::filesystem::path, 5> candidates{
        path,
        Utils::PathUtils::ToOsPath(path.string()),
        Utils::PathUtils::GetResourcePath(path.string()),
        Utils::PathUtils::GetSourcePath(path.string()),
        Utils::PathUtils::GetSourcePath(std::string("src/Lvs/Engine/Content/") + path.string())
    };
    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        if (Utils::FileIO::Exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

void ApplySkyboxFaceProcessing(
    Utils::ImageIO::ImageRgba8& image,
    const int resolutionCap,
    const bool compression
) {
    if (resolutionCap > 0) {
        const int maxSide = std::max(static_cast<int>(image.Width), static_cast<int>(image.Height));
        if (maxSide > resolutionCap) {
            image = Utils::ImageIO::ResizeRgba8(
                image,
                resolutionCap,
                resolutionCap,
                true,
                true
            );
        }
    }

    if (compression) {
        image = Utils::ImageIO::ResizeRgba8(
            image,
            std::max(1, static_cast<int>(image.Width) / 2),
            std::max(1, static_cast<int>(image.Height) / 2),
            false,
            false
        );
    }
}

RHI::CubemapDesc BuildFromFaces(
    std::array<Utils::ImageIO::ImageRgba8, 6>& faces,
    const SkyboxSettingsSnapshot& settings
) {
    for (auto& image : faces) {
        ApplySkyboxFaceProcessing(image, settings.ResolutionCap, settings.Compression);
    }

    std::uint32_t targetEdge = UINT32_MAX;
    for (const auto& face : faces) {
        const std::uint32_t faceEdge = std::max(face.Width, face.Height);
        if (faceEdge == 0U) {
            throw std::runtime_error("Skybox face dimensions are invalid.");
        }
        targetEdge = std::min(targetEdge, faceEdge);
    }
    if (targetEdge == UINT32_MAX || targetEdge == 0U) {
        throw std::runtime_error("Skybox face dimensions are invalid.");
    }

    // Normalize all faces to one common square size by fitting each source
    // image into targetEdge x targetEdge while preserving aspect ratio.
    // This avoids runtime failures/stutter when individual face textures have
    // mixed resolutions.
    for (auto& face : faces) {
        auto fitted = Utils::ImageIO::ResizeRgba8(
            face,
            static_cast<int>(targetEdge),
            static_cast<int>(targetEdge),
            true,
            true
        );
        if (fitted.Width == targetEdge && fitted.Height == targetEdge) {
            face = std::move(fitted);
            continue;
        }

        Utils::ImageIO::ImageRgba8 canvas{};
        canvas.Width = targetEdge;
        canvas.Height = targetEdge;
        canvas.Pixels.assign(static_cast<std::size_t>(targetEdge) * static_cast<std::size_t>(targetEdge) * 4U, 0U);

        const std::uint32_t offsetX = (targetEdge - fitted.Width) / 2U;
        const std::uint32_t offsetY = (targetEdge - fitted.Height) / 2U;
        for (std::uint32_t y = 0; y < fitted.Height; ++y) {
            const auto srcRowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(fitted.Width) * 4U;
            const auto dstRowOffset = (static_cast<std::size_t>(y + offsetY) * static_cast<std::size_t>(targetEdge) +
                                       static_cast<std::size_t>(offsetX)) *
                                      4U;
            std::copy_n(
                fitted.Pixels.data() + srcRowOffset,
                static_cast<std::size_t>(fitted.Width) * 4U,
                canvas.Pixels.data() + dstRowOffset
            );
        }
        face = std::move(canvas);
    }

    RHI::CubemapDesc desc{};
    desc.width = targetEdge;
    desc.height = targetEdge;
    desc.format = RHI::Format::R8G8B8A8_UNorm;
    desc.linearFiltering = settings.Filtering == Enums::TextureFiltering::Linear;
    for (std::size_t i = 0; i < faces.size(); ++i) {
        desc.faces[i] = std::move(faces[i].Pixels);
    }
    return desc;
}

std::array<std::filesystem::path, 6> DefaultIndividualSkyboxPaths() {
    return {
        Utils::PathUtils::GetResourcePath("Sky/nullsky/RT.png"),
        Utils::PathUtils::GetResourcePath("Sky/nullsky/LF.png"),
        Utils::PathUtils::GetResourcePath("Sky/nullsky/UP.png"),
        Utils::PathUtils::GetResourcePath("Sky/nullsky/DN.png"),
        Utils::PathUtils::GetResourcePath("Sky/nullsky/FT.png"),
        Utils::PathUtils::GetResourcePath("Sky/nullsky/BK.png")
    };
}

std::filesystem::path DefaultCrossSkyboxPath() {
    return Utils::PathUtils::GetResourcePath("Sky/nullcrosssky/sky512.png");
}

RHI::CubemapDesc LoadFromCrossPath(const std::filesystem::path& crossTexturePath, const SkyboxSettingsSnapshot& settings) {
    const auto crossPath = ResolvePath(crossTexturePath);
    if (crossPath.empty()) {
        throw std::runtime_error("Skybox cross texture path is not available.");
    }
    const auto cross = Utils::ImageIO::LoadRgba8(crossPath);
    if (cross.Width % 4 != 0 || cross.Height % 3 != 0) {
        throw std::runtime_error("Skybox cross texture dimensions must be divisible by 4x3.");
    }
    const int faceWidth = static_cast<int>(cross.Width) / 4;
    const int faceHeight = static_cast<int>(cross.Height) / 3;
    if (faceWidth <= 0 || faceHeight <= 0 || faceWidth != faceHeight) {
        throw std::runtime_error("Skybox cross texture must contain square faces.");
    }

    std::array<Utils::ImageIO::ImageRgba8, 6> faces{
        Utils::ImageIO::CropRgba8(cross, faceWidth * 2, faceHeight, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 0, faceHeight, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 1, faceHeight * 0, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 1, faceHeight * 2, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 1, faceHeight, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 3, faceHeight, faceWidth, faceHeight)
    };
    return BuildFromFaces(faces, settings);
}

} // namespace

RHI::CubemapDesc CubemapLoader::LoadFromSkyboxSettings(const SkyboxSettingsSnapshot& settings) {
    if (settings.TextureLayout == Enums::SkyboxTextureLayout::Cross) {
        try {
            return LoadFromCrossPath(settings.CrossTexture, settings);
        } catch (const std::exception&) {
            return LoadFromCrossPath(DefaultCrossSkyboxPath(), settings);
        }
    }

    std::array<Utils::ImageIO::ImageRgba8, 6> faces{};
    // Resolve each face independently so runtime path changes apply even if
    // only a subset of individual faces are overridden.
    const auto defaults = DefaultIndividualSkyboxPaths();
    for (std::size_t i = 0; i < defaults.size(); ++i) {
        auto path = ResolvePath(settings.Faces[i]);
        if (path.empty()) {
            path = ResolvePath(defaults[i]);
        }
        if (path.empty()) {
            throw std::runtime_error("Skybox face texture path is not available.");
        }
        faces[i] = Utils::ImageIO::LoadRgba8(path);
    }
    return BuildFromFaces(faces, settings);
}

} // namespace Lvs::Engine::Rendering::Common
