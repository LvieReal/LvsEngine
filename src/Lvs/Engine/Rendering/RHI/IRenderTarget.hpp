#pragma once

#include "Lvs/Engine/Rendering/RHI/Texture.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

namespace Lvs::Engine::Rendering::RHI {

struct RenderTargetDesc {
    u32 width{0};
    u32 height{0};
    u32 colorAttachmentCount{1};
    bool hasDepth{true};
};

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    [[nodiscard]] virtual void* GetRenderPassHandle() const = 0;
    [[nodiscard]] virtual void* GetFramebufferHandle() const = 0;
    [[nodiscard]] virtual u32 GetWidth() const = 0;
    [[nodiscard]] virtual u32 GetHeight() const = 0;
    [[nodiscard]] virtual u32 GetColorAttachmentCount() const = 0;
    [[nodiscard]] virtual Texture GetColorTexture(u32 index) const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI

