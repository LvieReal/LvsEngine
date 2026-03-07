#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Lvs::Engine::Utils::ImageIO {

struct ImageRgba8 {
    std::uint32_t Width{0};
    std::uint32_t Height{0};
    std::vector<std::uint8_t> Pixels;
};

[[nodiscard]] ImageRgba8 LoadRgba8(const std::filesystem::path& path);
[[nodiscard]] ImageRgba8 ResizeRgba8(
    const ImageRgba8& image,
    int width,
    int height,
    bool keepAspect,
    bool smooth
);
[[nodiscard]] ImageRgba8 CropRgba8(const ImageRgba8& image, int x, int y, int width, int height);

} // namespace Lvs::Engine::Utils::ImageIO
