#include "Lvs/Engine/Rendering/Common/Primitives.hpp"

#include <algorithm>
#include <cmath>

namespace Lvs::Engine::Rendering::Common::Primitives {

namespace {
constexpr double PI = 3.14159265358979323846;

using Vertex = VertexP3N3;

Vertex MakeVertex(const float x, const float y, const float z, const float nx, const float ny, const float nz) {
    return Vertex{
        .Position = {x, y, z},
        .Normal = {nx, ny, nz}
    };
}

} // namespace

MeshData GenerateCube() {
    MeshData data;
    data.Vertices = {
        MakeVertex(-0.5F, -0.5F, 0.5F, 0.0F, 0.0F, 1.0F),
        MakeVertex(0.5F, -0.5F, 0.5F, 0.0F, 0.0F, 1.0F),
        MakeVertex(0.5F, 0.5F, 0.5F, 0.0F, 0.0F, 1.0F),
        MakeVertex(-0.5F, 0.5F, 0.5F, 0.0F, 0.0F, 1.0F),
        MakeVertex(0.5F, -0.5F, -0.5F, 0.0F, 0.0F, -1.0F),
        MakeVertex(-0.5F, -0.5F, -0.5F, 0.0F, 0.0F, -1.0F),
        MakeVertex(-0.5F, 0.5F, -0.5F, 0.0F, 0.0F, -1.0F),
        MakeVertex(0.5F, 0.5F, -0.5F, 0.0F, 0.0F, -1.0F),
        MakeVertex(-0.5F, -0.5F, -0.5F, -1.0F, 0.0F, 0.0F),
        MakeVertex(-0.5F, -0.5F, 0.5F, -1.0F, 0.0F, 0.0F),
        MakeVertex(-0.5F, 0.5F, 0.5F, -1.0F, 0.0F, 0.0F),
        MakeVertex(-0.5F, 0.5F, -0.5F, -1.0F, 0.0F, 0.0F),
        MakeVertex(0.5F, -0.5F, 0.5F, 1.0F, 0.0F, 0.0F),
        MakeVertex(0.5F, -0.5F, -0.5F, 1.0F, 0.0F, 0.0F),
        MakeVertex(0.5F, 0.5F, -0.5F, 1.0F, 0.0F, 0.0F),
        MakeVertex(0.5F, 0.5F, 0.5F, 1.0F, 0.0F, 0.0F),
        MakeVertex(-0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F),
        MakeVertex(0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F),
        MakeVertex(0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F),
        MakeVertex(-0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F),
        MakeVertex(-0.5F, -0.5F, -0.5F, 0.0F, -1.0F, 0.0F),
        MakeVertex(0.5F, -0.5F, -0.5F, 0.0F, -1.0F, 0.0F),
        MakeVertex(0.5F, -0.5F, 0.5F, 0.0F, -1.0F, 0.0F),
        MakeVertex(-0.5F, -0.5F, 0.5F, 0.0F, -1.0F, 0.0F),
    };

    data.Indices = {
        0, 1, 2, 2, 3, 0,       4, 5, 6, 6, 7, 4,       8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
    };

    return data;
}

MeshData GenerateSphere(int rings, int segments) {
    rings = std::max(3, rings);
    segments = std::max(3, segments);

    MeshData data;
    data.Vertices.reserve(static_cast<std::size_t>(rings * segments + 2));
    data.Indices.reserve(static_cast<std::size_t>(rings * segments * 6));

    data.Vertices.push_back(MakeVertex(0.0F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F));

    for (int r = 1; r < rings; ++r) {
        const float theta = static_cast<float>((static_cast<double>(r) / static_cast<double>(rings)) * PI);
        const float sinT = std::sin(theta);
        const float cosT = std::cos(theta);
        for (int s = 0; s < segments; ++s) {
            const float phi = static_cast<float>((static_cast<double>(s) / static_cast<double>(segments)) * 2.0 * PI);
            const float sinP = std::sin(phi);
            const float cosP = std::cos(phi);
            const float x = sinT * cosP;
            const float y = cosT;
            const float z = sinT * sinP;
            data.Vertices.push_back(MakeVertex(x * 0.5F, y * 0.5F, z * 0.5F, x, y, z));
        }
    }

    const std::uint32_t bottomIndex = static_cast<std::uint32_t>(data.Vertices.size());
    data.Vertices.push_back(MakeVertex(0.0F, -0.5F, 0.0F, 0.0F, -1.0F, 0.0F));

    auto ringStart = [segments](const int ring) -> std::uint32_t {
        return 1U + static_cast<std::uint32_t>((ring - 1) * segments);
    };

    const std::uint32_t firstRing = ringStart(1);
    for (int s = 0; s < segments; ++s) {
        const std::uint32_t a = firstRing + static_cast<std::uint32_t>(s);
        const std::uint32_t b = firstRing + static_cast<std::uint32_t>((s + 1) % segments);
        data.Indices.insert(data.Indices.end(), {0U, b, a});
    }

    for (int r = 1; r < rings - 1; ++r) {
        const std::uint32_t r0 = ringStart(r);
        const std::uint32_t r1 = ringStart(r + 1);
        for (int s = 0; s < segments; ++s) {
            const std::uint32_t a = r0 + static_cast<std::uint32_t>(s);
            const std::uint32_t b = r0 + static_cast<std::uint32_t>((s + 1) % segments);
            const std::uint32_t c = r1 + static_cast<std::uint32_t>(s);
            const std::uint32_t d = r1 + static_cast<std::uint32_t>((s + 1) % segments);
            data.Indices.insert(data.Indices.end(), {a, d, c, a, b, d});
        }
    }

    const std::uint32_t lastRing = ringStart(rings - 1);
    for (int s = 0; s < segments; ++s) {
        const std::uint32_t a = lastRing + static_cast<std::uint32_t>(s);
        const std::uint32_t b = lastRing + static_cast<std::uint32_t>((s + 1) % segments);
        data.Indices.insert(data.Indices.end(), {bottomIndex, a, b});
    }

    return data;
}

MeshData GenerateCylinder(const int segments, const bool caps) {
    const int safeSegments = std::max(3, segments);
    MeshData data;

    for (int i = 0; i < safeSegments; ++i) {
        const float angle = static_cast<float>((2.0 * PI * i) / static_cast<double>(safeSegments));
        const float x = std::cos(angle) * 0.5F;
        const float z = std::sin(angle) * 0.5F;
        data.Vertices.push_back(MakeVertex(x, -0.5F, z, x, 0.0F, z));
        data.Vertices.push_back(MakeVertex(x, 0.5F, z, x, 0.0F, z));
    }

    for (int i = 0; i < safeSegments; ++i) {
        const std::uint32_t i0 = static_cast<std::uint32_t>(i * 2);
        const std::uint32_t i1 = static_cast<std::uint32_t>((i * 2 + 2) % (safeSegments * 2));
        data.Indices.insert(data.Indices.end(), {i1, i0, i0 + 1, i1 + 1, i1, i0 + 1});
    }

    if (caps) {
        const std::uint32_t bottomCenter = static_cast<std::uint32_t>(data.Vertices.size());
        data.Vertices.push_back(MakeVertex(0.0F, -0.5F, 0.0F, 0.0F, -1.0F, 0.0F));
        for (int i = 0; i < safeSegments; ++i) {
            const std::uint32_t i1 = static_cast<std::uint32_t>(i * 2);
            const std::uint32_t i2 = static_cast<std::uint32_t>((i * 2 + 2) % (safeSegments * 2));
            data.Indices.insert(data.Indices.end(), {bottomCenter, i1, i2});
        }

        const std::uint32_t topCenter = static_cast<std::uint32_t>(data.Vertices.size());
        data.Vertices.push_back(MakeVertex(0.0F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F));
        for (int i = 0; i < safeSegments; ++i) {
            const std::uint32_t i1 = static_cast<std::uint32_t>(i * 2 + 1);
            const std::uint32_t i2 = static_cast<std::uint32_t>((i * 2 + 3) % (safeSegments * 2));
            data.Indices.insert(data.Indices.end(), {topCenter, i2, i1});
        }
    }

    return data;
}

MeshData GenerateCone(const int segments, const bool caps) {
    const int safeSegments = std::max(3, segments);
    MeshData data;

    data.Vertices.push_back(MakeVertex(0.0F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F));
    for (int i = 0; i < safeSegments; ++i) {
        const float angle = static_cast<float>((2.0 * PI * i) / static_cast<double>(safeSegments));
        const float x = std::cos(angle) * 0.5F;
        const float z = std::sin(angle) * 0.5F;
        float nx = x;
        float ny = 0.5F;
        float nz = z;
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        nx /= len;
        ny /= len;
        nz /= len;
        data.Vertices.push_back(MakeVertex(x, -0.5F, z, nx, ny, nz));
    }

    for (int i = 0; i < safeSegments; ++i) {
        const std::uint32_t i1 = 1U + static_cast<std::uint32_t>(i);
        const std::uint32_t i2 = 1U + static_cast<std::uint32_t>((i + 1) % safeSegments);
        data.Indices.insert(data.Indices.end(), {0U, i2, i1});
    }

    if (caps) {
        const std::uint32_t baseCenter = static_cast<std::uint32_t>(data.Vertices.size());
        data.Vertices.push_back(MakeVertex(0.0F, -0.5F, 0.0F, 0.0F, -1.0F, 0.0F));
        for (int i = 0; i < safeSegments; ++i) {
            const std::uint32_t i1 = 1U + static_cast<std::uint32_t>(i);
            const std::uint32_t i2 = 1U + static_cast<std::uint32_t>((i + 1) % safeSegments);
            data.Indices.insert(data.Indices.end(), {baseCenter, i1, i2});
        }
    }

    return data;
}

} // namespace Lvs::Engine::Rendering::Common::Primitives
