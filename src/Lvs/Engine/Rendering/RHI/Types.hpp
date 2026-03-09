#pragma once

#include <cstdint>

namespace Lvs::Engine::Rendering::RHI {

using u32 = std::uint32_t;

enum class VertexLayout {
    None,
    P3,
    P3N3
};

enum class CullMode {
    None,
    Back,
    Front
};

enum class DepthCompare {
    Always,
    Equal,
    GreaterOrEqual,
    LessOrEqual
};

} // namespace Lvs::Engine::Rendering::RHI
