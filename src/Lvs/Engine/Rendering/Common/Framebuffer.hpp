#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"

namespace Lvs::Engine::Rendering::Common {

class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
    [[nodiscard]] virtual Rect GetRenderArea() const = 0;
    [[nodiscard]] virtual bool IsValid() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
