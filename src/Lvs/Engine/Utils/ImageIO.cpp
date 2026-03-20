#include "Lvs/Engine/Utils/ImageIO.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace Lvs::Engine::Utils::ImageIO {

namespace {

std::vector<std::uint8_t> ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open image: " + path.string());
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0) {
        return {};
    }
    in.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!in) {
        throw std::runtime_error("Failed to read image: " + path.string());
    }
    return bytes;
}

ImageRgba8 ResizeNearest(const ImageRgba8& image, const int width, const int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }
    if (image.Width == 0 || image.Height == 0 || image.Pixels.empty()) {
        return {};
    }

    ImageRgba8 out;
    out.Width = static_cast<std::uint32_t>(width);
    out.Height = static_cast<std::uint32_t>(height);
    out.Pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4U);

    for (int y = 0; y < height; ++y) {
        const int srcY = static_cast<int>((static_cast<double>(y) * static_cast<double>(image.Height)) / static_cast<double>(height));
        for (int x = 0; x < width; ++x) {
            const int srcX = static_cast<int>((static_cast<double>(x) * static_cast<double>(image.Width)) / static_cast<double>(width));

            const size_t srcIdx = (static_cast<size_t>(srcY) * static_cast<size_t>(image.Width) + static_cast<size_t>(srcX)) * 4U;
            const size_t dstIdx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4U;
            out.Pixels[dstIdx + 0] = image.Pixels[srcIdx + 0];
            out.Pixels[dstIdx + 1] = image.Pixels[srcIdx + 1];
            out.Pixels[dstIdx + 2] = image.Pixels[srcIdx + 2];
            out.Pixels[dstIdx + 3] = image.Pixels[srcIdx + 3];
        }
    }
    return out;
}

ImageRgba8 ResizeBilinear(const ImageRgba8& image, const int width, const int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }
    if (image.Width == 0 || image.Height == 0 || image.Pixels.empty()) {
        return {};
    }

    ImageRgba8 out;
    out.Width = static_cast<std::uint32_t>(width);
    out.Height = static_cast<std::uint32_t>(height);
    out.Pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4U);

    const double sx = static_cast<double>(image.Width) / static_cast<double>(width);
    const double sy = static_cast<double>(image.Height) / static_cast<double>(height);

    auto sample = [&](int x, int y, int c) -> double {
        x = std::clamp(x, 0, static_cast<int>(image.Width) - 1);
        y = std::clamp(y, 0, static_cast<int>(image.Height) - 1);
        return static_cast<double>(image.Pixels[(static_cast<size_t>(y) * static_cast<size_t>(image.Width) + static_cast<size_t>(x)) * 4U
                                               + static_cast<size_t>(c)]);
    };

    for (int y = 0; y < height; ++y) {
        const double fy = (static_cast<double>(y) + 0.5) * sy - 0.5;
        const int y0 = static_cast<int>(std::floor(fy));
        const int y1 = y0 + 1;
        const double ty = fy - static_cast<double>(y0);

        for (int x = 0; x < width; ++x) {
            const double fx = (static_cast<double>(x) + 0.5) * sx - 0.5;
            const int x0 = static_cast<int>(std::floor(fx));
            const int x1 = x0 + 1;
            const double tx = fx - static_cast<double>(x0);

            for (int c = 0; c < 4; ++c) {
                const double p00 = sample(x0, y0, c);
                const double p10 = sample(x1, y0, c);
                const double p01 = sample(x0, y1, c);
                const double p11 = sample(x1, y1, c);

                const double a = p00 + (p10 - p00) * tx;
                const double b = p01 + (p11 - p01) * tx;
                const double v = a + (b - a) * ty;

                out.Pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4U + static_cast<size_t>(c)]
                    = static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0));
            }
        }
    }
    return out;
}

} // namespace

ImageRgba8 LoadRgba8(const std::filesystem::path& path) {
    const auto bytes = ReadFileBytes(path);
    if (bytes.empty()) {
        throw std::runtime_error("Empty image file: " + path.string());
    }

    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_uc* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
    if (data == nullptr || w <= 0 || h <= 0) {
        throw std::runtime_error("Failed to decode image: " + path.string());
    }

    ImageRgba8 out;
    out.Width = static_cast<std::uint32_t>(w);
    out.Height = static_cast<std::uint32_t>(h);
    out.Pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4U);
    std::copy(data, data + out.Pixels.size(), out.Pixels.begin());
    stbi_image_free(data);
    return out;
}

ImageRgba8 ResizeRgba8(const ImageRgba8& image, const int width, const int height, const bool keepAspect, const bool smooth) {
    if (image.Width == 0 || image.Height == 0 || image.Pixels.empty()) {
        throw std::runtime_error("Cannot resize an empty image.");
    }
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid resize target.");
    }

    int targetW = width;
    int targetH = height;
    if (keepAspect) {
        const double sx = static_cast<double>(width) / static_cast<double>(image.Width);
        const double sy = static_cast<double>(height) / static_cast<double>(image.Height);
        const double s = std::min(sx, sy);
        targetW = std::max(1, static_cast<int>(std::round(static_cast<double>(image.Width) * s)));
        targetH = std::max(1, static_cast<int>(std::round(static_cast<double>(image.Height) * s)));
    }

    return smooth ? ResizeBilinear(image, targetW, targetH) : ResizeNearest(image, targetW, targetH);
}

ImageRgba8 CropRgba8(const ImageRgba8& image, const int x, const int y, const int width, const int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }
    if (image.Width == 0 || image.Height == 0 || image.Pixels.empty()) {
        throw std::runtime_error("Cannot crop an empty image.");
    }

    const int srcW = static_cast<int>(image.Width);
    const int srcH = static_cast<int>(image.Height);
    const int x0 = std::clamp(x, 0, srcW);
    const int y0 = std::clamp(y, 0, srcH);
    const int x1 = std::clamp(x + width, 0, srcW);
    const int y1 = std::clamp(y + height, 0, srcH);

    const int outW = std::max(0, x1 - x0);
    const int outH = std::max(0, y1 - y0);
    if (outW == 0 || outH == 0) {
        return {};
    }

    ImageRgba8 out;
    out.Width = static_cast<std::uint32_t>(outW);
    out.Height = static_cast<std::uint32_t>(outH);
    out.Pixels.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4U);

    for (int yy = 0; yy < outH; ++yy) {
        const size_t srcRow = (static_cast<size_t>(y0 + yy) * static_cast<size_t>(srcW) + static_cast<size_t>(x0)) * 4U;
        const size_t dstRow = static_cast<size_t>(yy) * static_cast<size_t>(outW) * 4U;
        std::copy_n(image.Pixels.begin() + static_cast<std::ptrdiff_t>(srcRow), static_cast<size_t>(outW) * 4U, out.Pixels.begin() + static_cast<std::ptrdiff_t>(dstRow));
    }

    return out;
}

} // namespace Lvs::Engine::Utils::ImageIO

