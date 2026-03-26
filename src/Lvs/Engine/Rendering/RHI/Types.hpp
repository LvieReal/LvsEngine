#pragma once

#include <cstdint>

namespace Lvs::Engine::Rendering::RHI {

using u32 = std::uint32_t;

enum class PrimitiveTopology {
    TriangleList,
    TriangleListAdjacency
};

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
    NotEqual,
    Less,
    Greater,
    GreaterOrEqual,
    LessOrEqual
};

enum class StencilCompare {
    Never,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Equal,
    NotEqual,
    Always
};

enum class StencilOp {
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap
};

struct StencilFaceState {
    StencilOp FailOp{StencilOp::Keep};
    StencilOp DepthFailOp{StencilOp::Keep};
    StencilOp PassOp{StencilOp::Keep};
    StencilCompare CompareOp{StencilCompare::Always};
    u32 CompareMask{0xFFu};
    u32 WriteMask{0xFFu};
    u32 Reference{0u};
};

struct StencilState {
    bool Enabled{false};
    StencilFaceState Front{};
    StencilFaceState Back{};
};

} // namespace Lvs::Engine::Rendering::RHI
