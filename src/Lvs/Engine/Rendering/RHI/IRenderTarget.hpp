#pragma once

#include "Lvs/Engine/Rendering/RHI/Format.hpp"
#include "Lvs/Engine/Rendering/RHI/Texture.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

namespace Lvs::Engine::Rendering::RHI {

struct RenderTargetDesc {
    u32 width{0};
    u32 height{0};
    u32 colorAttachmentCount{1};
    u32 sampleCount{1};
    bool hasDepth{true};
    bool depthTexture{false};
    Format depthFormat{Format::D32_Float};
};

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    [[nodiscard]] virtual void* GetRenderPassHandle() const = 0;
    [[nodiscard]] virtual void* GetFramebufferHandle() const = 0;
    [[nodiscard]] virtual u32 GetWidth() const = 0;
    [[nodiscard]] virtual u32 GetHeight() const = 0;
    [[nodiscard]] virtual u32 GetColorAttachmentCount() const = 0;
    [[nodiscard]] virtual u32 GetSampleCount() const = 0;
    [[nodiscard]] virtual Texture GetColorTexture(u32 index) const = 0;
    [[nodiscard]] virtual bool HasDepth() const = 0;
    [[nodiscard]] virtual Texture GetDepthTexture() const = 0;
};

} // namespace Lvs::Engine::Rendering::RHI
