#pragma once

#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <string>

namespace Lvs::Engine::Rendering::RHI {

struct PipelineDesc {
    std::string pipelineId{"main"};
    VertexLayout vertexLayout{VertexLayout::None};
    bool depthTest{true};
    bool depthWrite{true};
    DepthCompare depthCompare{DepthCompare::GreaterOrEqual};
    bool blending{false};
    CullMode cullMode{CullMode::Back};
};

class IPipeline {
public:
    virtual ~IPipeline() = default;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
