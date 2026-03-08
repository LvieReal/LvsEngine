#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/MeshCache.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Common/RenderSurface.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Rendering::Common {

class RenderProxy;

class SceneRenderer {
public:
    virtual ~SceneRenderer() = default;

    virtual void Initialize(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void RecreateSwapchain(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void DestroySwapchainResources(GraphicsContext& context, const RenderSurface& surface) = 0;
    virtual void Shutdown(GraphicsContext& context) = 0;

    virtual void BindToPlace(const std::shared_ptr<DataModel::Place>& place) = 0;
    virtual void Unbind() = 0;
    virtual void SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives) = 0;

    [[nodiscard]] virtual MeshCache& GetMeshCache() = 0;
    virtual void DrawRenderProxy(CommandBuffer& commandBuffer, const RenderProxy& proxy, bool transparent = false) = 0;
    virtual void RecordFrameCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t imageIndex,
        std::uint32_t frameIndex,
        const std::array<float, 4>& clearColor
    ) = 0;
    virtual void RecordShadowCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) = 0;
    virtual void RecordDrawCommands(
        GraphicsContext& context,
        const RenderSurface& surface,
        CommandBuffer& commandBuffer,
        std::uint32_t frameIndex
    ) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
