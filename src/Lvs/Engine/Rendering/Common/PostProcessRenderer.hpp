#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSurface.hpp"

#include <cstdint>
#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Rendering::Common {

class PostProcessRenderer {
public:
    virtual ~PostProcessRenderer() = default;

    virtual void Initialize(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void RecreateSwapchain(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void DestroySwapchainResources(GraphicsContext& context) = 0;
    virtual void Shutdown(GraphicsContext& context) = 0;

    virtual void BindToPlace(const std::shared_ptr<DataModel::Place>& place) = 0;
    virtual void Unbind() = 0;

    virtual void RecordBlurCommands(GraphicsContext& context, CommandBuffer& commandBuffer, std::uint32_t imageIndex) = 0;
    virtual void DrawComposite(
        GraphicsContext& context,
        CommandBuffer& commandBuffer,
        std::uint32_t imageIndex,
        std::uint32_t frameIndex
    ) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
