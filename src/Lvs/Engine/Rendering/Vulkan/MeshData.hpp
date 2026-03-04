#pragma once

#include "Lvs/Engine/Rendering/Vulkan/Vertex.hpp"

#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

struct MeshData {
    std::vector<Vertex> Vertices;
    std::vector<std::uint32_t> Indices;
};

} // namespace Lvs::Engine::Rendering::Vulkan
