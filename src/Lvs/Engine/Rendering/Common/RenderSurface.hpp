#pragma once

#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

struct Extent2D {
    std::uint32_t Width{0};
    std::uint32_t Height{0};
};

class RenderSurface {
public:
    virtual ~RenderSurface() = default;

    [[nodiscard]] virtual Extent2D GetExtent() const = 0;
    [[nodiscard]] virtual std::uint32_t GetFramesInFlight() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
