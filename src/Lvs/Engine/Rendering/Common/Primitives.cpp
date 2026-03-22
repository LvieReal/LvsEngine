#include "Lvs/Engine/Rendering/Common/Primitives.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <unordered_map>

namespace Lvs::Engine::Rendering::Common::Primitives {

namespace {
constexpr double PI = 3.14159265358979323846;

using Vertex = VertexP3N3;

struct Vec3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Normalize(const Vec3& v) {
    const float len2 = Dot(v, v);
    if (len2 <= 0.0F) {
        return Vec3{0.0F, 1.0F, 0.0F};
    }
    const float invLen = 1.0F / std::sqrt(len2);
    return Vec3{v.x * invLen, v.y * invLen, v.z * invLen};
}

Vertex MakeVertex(const float x, const float y, const float z, const float nx, const float ny, const float nz) {
    return Vertex{
        .Position = {x, y, z},
        .Normal = {nx, ny, nz}
    };
}

struct Triangle {
    Vec3 P0{};
    Vec3 P1{};
    Vec3 P2{};
    Vec3 N{};
};

Triangle MakeTriangle(Vec3 a, Vec3 b, Vec3 c, const Vec3& expectedOut) {
    Vec3 n = Cross(b - a, c - a);
    if (Dot(n, expectedOut) < 0.0F) {
        std::swap(b, c);
        n = Cross(b - a, c - a);
    }
    return Triangle{a, b, c, Normalize(n)};
}

void AddQuad(
    std::vector<Triangle>& out,
    const Vec3& p0,
    const Vec3& p1,
    const Vec3& p2,
    const Vec3& p3,
    const Vec3& expectedOut
) {
    out.push_back(MakeTriangle(p0, p1, p2, expectedOut));
    out.push_back(MakeTriangle(p2, p3, p0, expectedOut));
}

void AddTriangleFace(std::vector<Triangle>& out, const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& expectedOut) {
    out.push_back(MakeTriangle(p0, p1, p2, expectedOut));
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

MeshData GenerateBeveledCube(const float sizeX, const float sizeY, const float sizeZ, const float bevelWidthWorld, const bool smoothNormals) {
    if (bevelWidthWorld <= 0.0F || sizeX <= 0.0F || sizeY <= 0.0F || sizeZ <= 0.0F) {
        return GenerateCube();
    }

    constexpr float half = 0.5F;
    constexpr float eps = 1e-4F;

    const float wx = std::min(std::max(bevelWidthWorld / sizeX, 0.0F), half - eps);
    const float wy = std::min(std::max(bevelWidthWorld / sizeY, 0.0F), half - eps);
    const float wz = std::min(std::max(bevelWidthWorld / sizeZ, 0.0F), half - eps);

    const float ix = half - wx;
    const float iy = half - wy;
    const float iz = half - wz;

    std::vector<Triangle> mainTris;
    std::vector<Triangle> bevelTris;
    mainTris.reserve(12);
    bevelTris.reserve(32);

    // Main faces (keep hard edges against bevel).
    // +X / -X
    AddQuad(
        mainTris,
        Vec3{half, -iy, -iz}, Vec3{half, iy, -iz}, Vec3{half, iy, iz}, Vec3{half, -iy, iz},
        Vec3{1.0F, 0.0F, 0.0F}
    );
    AddQuad(
        mainTris,
        Vec3{-half, -iy, iz}, Vec3{-half, iy, iz}, Vec3{-half, iy, -iz}, Vec3{-half, -iy, -iz},
        Vec3{-1.0F, 0.0F, 0.0F}
    );
    // +Y / -Y
    AddQuad(
        mainTris,
        Vec3{-ix, half, -iz}, Vec3{ix, half, -iz}, Vec3{ix, half, iz}, Vec3{-ix, half, iz},
        Vec3{0.0F, 1.0F, 0.0F}
    );
    AddQuad(
        mainTris,
        Vec3{-ix, -half, iz}, Vec3{ix, -half, iz}, Vec3{ix, -half, -iz}, Vec3{-ix, -half, -iz},
        Vec3{0.0F, -1.0F, 0.0F}
    );
    // +Z / -Z
    AddQuad(
        mainTris,
        Vec3{-ix, -iy, half}, Vec3{ix, -iy, half}, Vec3{ix, iy, half}, Vec3{-ix, iy, half},
        Vec3{0.0F, 0.0F, 1.0F}
    );
    AddQuad(
        mainTris,
        Vec3{-ix, iy, -half}, Vec3{ix, iy, -half}, Vec3{ix, -iy, -half}, Vec3{-ix, -iy, -half},
        Vec3{0.0F, 0.0F, -1.0F}
    );

    // Edge bevel faces (12 quads).
    // Edges parallel to Z between X and Y faces.
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            const Vec3 expected{static_cast<float>(sx), static_cast<float>(sy), 0.0F};
            const float xh = static_cast<float>(sx) * half;
            const float yh = static_cast<float>(sy) * half;
            const float xi = static_cast<float>(sx) * ix;
            const float yi = static_cast<float>(sy) * iy;
            AddQuad(
                bevelTris,
                Vec3{xh, yi, -iz},
                Vec3{xi, yh, -iz},
                Vec3{xi, yh, iz},
                Vec3{xh, yi, iz},
                expected
            );
        }
    }

    // Edges parallel to Y between X and Z faces.
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            const Vec3 expected{static_cast<float>(sx), 0.0F, static_cast<float>(sz)};
            const float xh = static_cast<float>(sx) * half;
            const float zh = static_cast<float>(sz) * half;
            const float xi = static_cast<float>(sx) * ix;
            const float zi = static_cast<float>(sz) * iz;
            AddQuad(
                bevelTris,
                Vec3{xh, -iy, zi},
                Vec3{xi, -iy, zh},
                Vec3{xi, iy, zh},
                Vec3{xh, iy, zi},
                expected
            );
        }
    }

    // Edges parallel to X between Y and Z faces.
    for (int sy = -1; sy <= 1; sy += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            const Vec3 expected{0.0F, static_cast<float>(sy), static_cast<float>(sz)};
            const float yh = static_cast<float>(sy) * half;
            const float zh = static_cast<float>(sz) * half;
            const float yi = static_cast<float>(sy) * iy;
            const float zi = static_cast<float>(sz) * iz;
            AddQuad(
                bevelTris,
                Vec3{-ix, yh, zi},
                Vec3{-ix, yi, zh},
                Vec3{ix, yi, zh},
                Vec3{ix, yh, zi},
                expected
            );
        }
    }

    // Corner bevel faces (8 triangles).
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                const Vec3 expected{static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)};
                const float xh = static_cast<float>(sx) * half;
                const float yh = static_cast<float>(sy) * half;
                const float zh = static_cast<float>(sz) * half;
                const float xi = static_cast<float>(sx) * ix;
                const float yi = static_cast<float>(sy) * iy;
                const float zi = static_cast<float>(sz) * iz;
                AddTriangleFace(
                    bevelTris,
                    Vec3{xh, yi, zi},
                    Vec3{xi, yh, zi},
                    Vec3{xi, yi, zh},
                    expected
                );
            }
        }
    }

    MeshData data;

    auto emitTriangleFlat = [&data](const Triangle& t) {
        const std::uint32_t base = static_cast<std::uint32_t>(data.Vertices.size());
        data.Vertices.push_back(MakeVertex(t.P0.x, t.P0.y, t.P0.z, t.N.x, t.N.y, t.N.z));
        data.Vertices.push_back(MakeVertex(t.P1.x, t.P1.y, t.P1.z, t.N.x, t.N.y, t.N.z));
        data.Vertices.push_back(MakeVertex(t.P2.x, t.P2.y, t.P2.z, t.N.x, t.N.y, t.N.z));
        data.Indices.insert(data.Indices.end(), {base, base + 1U, base + 2U});
    };

    if (!smoothNormals) {
        data.Vertices.reserve(static_cast<std::size_t>((mainTris.size() + bevelTris.size()) * 3));
        data.Indices.reserve(static_cast<std::size_t>((mainTris.size() + bevelTris.size()) * 3));
        for (const auto& t : mainTris) {
            emitTriangleFlat(t);
        }
        for (const auto& t : bevelTris) {
            emitTriangleFlat(t);
        }
        return data;
    }

    // Keep main faces flat, but smooth normals across bevel faces only.
    data.Vertices.reserve(static_cast<std::size_t>(mainTris.size() * 3) + 64);
    data.Indices.reserve(static_cast<std::size_t>((mainTris.size() + bevelTris.size()) * 3));

    for (const auto& t : mainTris) {
        emitTriangleFlat(t);
    }

    const std::uint32_t bevelStart = static_cast<std::uint32_t>(data.Vertices.size());

    struct PosKey {
        std::uint32_t x{};
        std::uint32_t y{};
        std::uint32_t z{};
        bool operator==(const PosKey& other) const = default;
    };

    struct PosKeyHash {
        std::size_t operator()(const PosKey& k) const noexcept {
            std::size_t h = static_cast<std::size_t>(k.x);
            h ^= static_cast<std::size_t>(k.y) + 0x9e3779b9 + (h << 6U) + (h >> 2U);
            h ^= static_cast<std::size_t>(k.z) + 0x9e3779b9 + (h << 6U) + (h >> 2U);
            return h;
        }
    };

    auto bits = [](float v) -> std::uint32_t {
        if (v == 0.0F) {
            v = 0.0F;
        }
        std::uint32_t out = 0;
        std::memcpy(&out, &v, sizeof(float));
        return out;
    };

    auto keyFor = [&bits](const Vec3& p) -> PosKey {
        return PosKey{bits(p.x), bits(p.y), bits(p.z)};
    };

    std::unordered_map<PosKey, std::uint32_t, PosKeyHash> indexByPos;
    indexByPos.reserve(128);

    std::vector<Vec3> normalAcc;
    normalAcc.resize(data.Vertices.size(), Vec3{0.0F, 0.0F, 0.0F});

    auto getOrCreateBevelVertex = [&data, &indexByPos, &normalAcc, &keyFor](const Vec3& p) -> std::uint32_t {
        const PosKey k = keyFor(p);
        if (const auto it = indexByPos.find(k); it != indexByPos.end()) {
            return it->second;
        }
        const std::uint32_t idx = static_cast<std::uint32_t>(data.Vertices.size());
        data.Vertices.push_back(MakeVertex(p.x, p.y, p.z, 0.0F, 1.0F, 0.0F));
        normalAcc.push_back(Vec3{0.0F, 0.0F, 0.0F});
        indexByPos.emplace(k, idx);
        return idx;
    };

    for (const auto& t : bevelTris) {
        const std::uint32_t i0 = getOrCreateBevelVertex(t.P0);
        const std::uint32_t i1 = getOrCreateBevelVertex(t.P1);
        const std::uint32_t i2 = getOrCreateBevelVertex(t.P2);
        data.Indices.insert(data.Indices.end(), {i0, i1, i2});
        normalAcc[i0].x += t.N.x;
        normalAcc[i0].y += t.N.y;
        normalAcc[i0].z += t.N.z;
        normalAcc[i1].x += t.N.x;
        normalAcc[i1].y += t.N.y;
        normalAcc[i1].z += t.N.z;
        normalAcc[i2].x += t.N.x;
        normalAcc[i2].y += t.N.y;
        normalAcc[i2].z += t.N.z;
    }

    for (std::uint32_t i = bevelStart; i < static_cast<std::uint32_t>(data.Vertices.size()); ++i) {
        const Vec3 n = Normalize(normalAcc[i]);
        data.Vertices[i].Normal[0] = n.x;
        data.Vertices[i].Normal[1] = n.y;
        data.Vertices[i].Normal[2] = n.z;
    }

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
