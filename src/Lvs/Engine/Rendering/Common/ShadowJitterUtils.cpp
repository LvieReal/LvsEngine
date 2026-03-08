#include "Lvs/Engine/Rendering/Common/ShadowJitterUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace Lvs::Engine::Rendering::Common {

namespace {

struct Offset {
    double X{0.0};
    double Y{0.0};
};

std::uint8_t EncodeOffsetComponent(const double value) {
    const double normalized = std::clamp((value * 0.5) + 0.5, 0.0, 1.0);
    return static_cast<std::uint8_t>(std::round(normalized * 255.0));
}

} // namespace

std::vector<std::uint8_t> GenerateShadowJitterTextureData(std::uint32_t sizeXY, std::uint32_t depth) {
    const int clampedSize = static_cast<int>(std::max(kShadowMinJitterSizeXY, sizeXY));
    const int clampedDepth = static_cast<int>(std::max(kShadowMinJitterDepth, depth));
    const int pairCount = clampedDepth * 2;
    int samplesPerSide = static_cast<int>(std::sqrt(static_cast<double>(pairCount)));
    if (samplesPerSide * samplesPerSide != pairCount) {
        samplesPerSide = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(pairCount))));
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(clampedSize) * clampedSize * clampedDepth * 4U, 0U);
    constexpr double kPi = 3.14159265358979323846;

    for (int y = 0; y < clampedSize; ++y) {
        for (int x = 0; x < clampedSize; ++x) {
            const std::uint32_t seed = static_cast<std::uint32_t>(((x + 1) * 73856093) ^ ((y + 1) * 19349663));
            std::mt19937 rng(seed);
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            std::vector<Offset> offsets;
            offsets.reserve(static_cast<std::size_t>(pairCount));
            for (int i = 0; i < pairCount; ++i) {
                const int sx = i % samplesPerSide;
                const int sy = i / samplesPerSide;
                const double u = (static_cast<double>(sx) + dist(rng)) / static_cast<double>(samplesPerSide);
                const double v = (static_cast<double>(sy) + dist(rng)) / static_cast<double>(samplesPerSide);
                const double radius = std::sqrt(std::clamp(v, 0.0, 1.0));
                const double theta = 2.0 * kPi * u;
                offsets.push_back({std::cos(theta) * radius, std::sin(theta) * radius});
            }
            std::sort(offsets.begin(), offsets.end(), [](const Offset& lhs, const Offset& rhs) {
                const double lhsRadius = (lhs.X * lhs.X) + (lhs.Y * lhs.Y);
                const double rhsRadius = (rhs.X * rhs.X) + (rhs.Y * rhs.Y);
                return lhsRadius > rhsRadius;
            });

            for (int z = 0; z < clampedDepth; ++z) {
                const auto& a = offsets[static_cast<std::size_t>((z * 2) + 0)];
                const auto& b = offsets[static_cast<std::size_t>((z * 2) + 1)];
                const std::size_t index = (
                    (static_cast<std::size_t>(z) * static_cast<std::size_t>(clampedSize) * static_cast<std::size_t>(clampedSize)) +
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(clampedSize)) +
                    static_cast<std::size_t>(x)
                ) * 4U;
                data[index + 0] = EncodeOffsetComponent(a.X);
                data[index + 1] = EncodeOffsetComponent(a.Y);
                data[index + 2] = EncodeOffsetComponent(b.X);
                data[index + 3] = EncodeOffsetComponent(b.Y);
            }
        }
    }

    return data;
}

} // namespace Lvs::Engine::Rendering::Common
