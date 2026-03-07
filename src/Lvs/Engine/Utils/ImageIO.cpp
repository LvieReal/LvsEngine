#include "Lvs/Engine/Utils/ImageIO.hpp"

#include <QImage>
#include <QString>

#include <cstring>
#include <stdexcept>

namespace Lvs::Engine::Utils::ImageIO {

namespace {

ImageRgba8 ToImageData(const QImage& image) {
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    ImageRgba8 result;
    result.Width = static_cast<std::uint32_t>(rgba.width());
    result.Height = static_cast<std::uint32_t>(rgba.height());
    result.Pixels.resize(static_cast<std::size_t>(rgba.width()) * static_cast<std::size_t>(rgba.height()) * 4U);
    std::memcpy(result.Pixels.data(), rgba.constBits(), result.Pixels.size());
    return result;
}

QImage ToQImage(const ImageRgba8& image) {
    if (image.Width == 0 || image.Height == 0 || image.Pixels.empty()) {
        return {};
    }
    QImage view(
        image.Pixels.data(),
        static_cast<int>(image.Width),
        static_cast<int>(image.Height),
        static_cast<int>(image.Width * 4U),
        QImage::Format_RGBA8888
    );
    return view.copy();
}

} // namespace

ImageRgba8 LoadRgba8(const std::filesystem::path& path) {
    QImage image(QString::fromStdWString(path.wstring()));
    if (image.isNull()) {
        throw std::runtime_error("Failed to load image: " + path.string());
    }
    return ToImageData(image);
}

ImageRgba8 ResizeRgba8(const ImageRgba8& image, const int width, const int height, const bool keepAspect, const bool smooth) {
    const QImage source = ToQImage(image);
    if (source.isNull()) {
        throw std::runtime_error("Cannot resize an empty image.");
    }
    const QImage resized = source.scaled(
        width,
        height,
        keepAspect ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio,
        smooth ? Qt::SmoothTransformation : Qt::FastTransformation
    );
    return ToImageData(resized);
}

ImageRgba8 CropRgba8(const ImageRgba8& image, const int x, const int y, const int width, const int height) {
    const QImage source = ToQImage(image);
    if (source.isNull()) {
        throw std::runtime_error("Cannot crop an empty image.");
    }
    return ToImageData(source.copy(x, y, width, height));
}

} // namespace Lvs::Engine::Utils::ImageIO
