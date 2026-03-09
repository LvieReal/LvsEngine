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
    if (Utils::FileIO::Exists(path)) {
        return path;
    }
    const auto converted = Utils::PathUtils::ToOsPath(path.string());
    if (Utils::FileIO::Exists(converted)) {
        return converted;
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

    const std::uint32_t width = faces[0].Width;
    const std::uint32_t height = faces[0].Height;
    if (width == 0 || height == 0) {
        throw std::runtime_error("Skybox face dimensions are invalid.");
    }
    for (std::size_t i = 1; i < faces.size(); ++i) {
        if (faces[i].Width != width || faces[i].Height != height) {
            throw std::runtime_error("Skybox faces must have identical dimensions.");
        }
    }

    RHI::CubemapDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = RHI::Format::R8G8B8A8_UNorm;
    desc.linearFiltering = settings.Filtering == Enums::TextureFiltering::Linear;
    for (std::size_t i = 0; i < faces.size(); ++i) {
        desc.faces[i] = std::move(faces[i].Pixels);
    }
    return desc;
}

} // namespace

RHI::CubemapDesc CubemapLoader::LoadFromSkyboxSettings(const SkyboxSettingsSnapshot& settings) {
    if (settings.TextureLayout == Enums::SkyboxTextureLayout::Cross) {
        const auto crossPath = ResolvePath(settings.CrossTexture);
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

    std::array<Utils::ImageIO::ImageRgba8, 6> faces{};
    for (std::size_t i = 0; i < settings.Faces.size(); ++i) {
        const auto path = ResolvePath(settings.Faces[i]);
        if (path.empty()) {
            throw std::runtime_error("Skybox face texture path is not available.");
        }
        faces[i] = Utils::ImageIO::LoadRgba8(path);
    }
    return BuildFromFaces(faces, settings);
}

} // namespace Lvs::Engine::Rendering::Common
