#pragma once

namespace Lvs::Engine::Rendering::Common {

class PipelineLayout {
public:
    virtual ~PipelineLayout() = default;

    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
    [[nodiscard]] virtual bool IsValid() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
