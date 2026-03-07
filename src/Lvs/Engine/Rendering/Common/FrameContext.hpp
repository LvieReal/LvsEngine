#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSurface.hpp"

#include <cstdint>

namespace Lvs::Engine::Rendering::Common {

struct FrameContext {
    GraphicsContext& Graphics;
    const RenderSurface& Surface;
    CommandBuffer& Commands;
    std::uint32_t FrameIndex{0};
};

} // namespace Lvs::Engine::Rendering::Common
