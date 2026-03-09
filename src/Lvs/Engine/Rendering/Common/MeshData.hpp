#pragma once

#include "Lvs/Engine/Rendering/Common/Vertex.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <vector>

namespace Lvs::Engine::Rendering::Common {

struct MeshData {
    std::vector<VertexP3N3> Vertices{};
    std::vector<RHI::u32> Indices{};
};

} // namespace Lvs::Engine::Rendering::Common
