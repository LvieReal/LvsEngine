#pragma once

#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <string>
#include <array>
#include <cstdint>

namespace Lvs::Engine::Rendering::RHI {

struct PipelineDesc {
    std::string pipelineId{"main"};
    VertexLayout vertexLayout{VertexLayout::None};
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    void* renderPassHandle{nullptr};
    u32 colorAttachmentCount{1};
    u32 sampleCount{1};
    bool depthTest{true};
    bool depthWrite{true};
    DepthCompare depthCompare{DepthCompare::GreaterOrEqual};
    bool blending{false};
    CullMode cullMode{CullMode::Back};
    bool colorWrite{true};
    bool useColorWriteMasks{false};
    std::array<std::uint8_t, 8> colorWriteMasks{{
        0xFU, 0xFU, 0xFU, 0xFU, 0xFU, 0xFU, 0xFU, 0xFU
    }};
    bool depthClamp{false};
    bool conservativeRaster{false};
    StencilState stencil{};
};

class IPipeline {
public:
    virtual ~IPipeline() = default;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
