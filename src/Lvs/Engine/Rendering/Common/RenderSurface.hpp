#pragma once

#include "Lvs/Engine/Rendering/Common/Framebuffer.hpp"
#include "Lvs/Engine/Rendering/Common/RenderPass.hpp"

#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

struct Extent2D {
    std::uint32_t Width{0};
    std::uint32_t Height{0};
};

struct NativeSampledImage {
    void* Image{nullptr};
    void* View{nullptr};
    void* Sampler{nullptr};
};

class RenderSurface {
public:
    virtual ~RenderSurface() = default;

    [[nodiscard]] virtual Extent2D GetExtent() const = 0;
    [[nodiscard]] virtual std::uint32_t GetImageCount() const = 0;
    [[nodiscard]] virtual std::uint32_t GetFramesInFlight() const = 0;
    [[nodiscard]] virtual const RenderPass& GetSceneRenderPass() const = 0;
    [[nodiscard]] virtual const RenderPass& GetPostProcessRenderPass() const = 0;
    [[nodiscard]] virtual const Framebuffer& GetSceneFramebuffer(std::uint32_t imageIndex) const = 0;
    [[nodiscard]] virtual const Framebuffer& GetSwapchainFramebuffer(std::uint32_t imageIndex) const = 0;
    [[nodiscard]] virtual NativeSampledImage GetOffscreenColorImage(std::uint32_t imageIndex) const = 0;
    [[nodiscard]] virtual NativeSampledImage GetOffscreenGlowImage(std::uint32_t imageIndex) const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
