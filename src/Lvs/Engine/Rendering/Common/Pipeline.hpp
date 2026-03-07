#pragma once

#include <memory>

namespace Lvs::Engine::Rendering::Common {

class CommandBuffer;
class PipelineLayout;

class Pipeline {
public:
    virtual ~Pipeline() = default;

    virtual void Bind(CommandBuffer& commandBuffer) const = 0;
    [[nodiscard]] virtual const PipelineLayout& GetLayout() const = 0;
    [[nodiscard]] virtual bool IsValid() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
