#pragma once

#include "Lvs/Engine/Rendering/RHI/IPipeline.hpp"

#include <functional>

namespace Lvs::Engine::Rendering::Backends::OpenGL {

class GLPipeline final : public RHI::IPipeline {
public:
    explicit GLPipeline(
        RHI::PipelineDesc desc,
        unsigned int programHandle = 0U,
        std::function<void(unsigned int)> destroy = nullptr
    );
    ~GLPipeline() override;

    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] const RHI::PipelineDesc& GetDesc() const;

private:
    RHI::PipelineDesc desc_{};
    unsigned int programHandle_{0U};
    std::function<void(unsigned int)> destroy_{};
};

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
