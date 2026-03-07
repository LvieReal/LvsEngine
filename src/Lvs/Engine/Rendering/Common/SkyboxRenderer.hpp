#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/ResourceBinding.hpp"

#include <cstdint>
#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Objects {
class Camera;
}

namespace Lvs::Engine::Rendering::Common {

class SkyboxRenderer {
public:
    virtual ~SkyboxRenderer() = default;

    virtual void Initialize(GraphicsContext& context) = 0;
    virtual void RecreateSwapchain(GraphicsContext& context) = 0;
    virtual void Shutdown(GraphicsContext& context) = 0;

    virtual void BindToPlace(const std::shared_ptr<DataModel::Place>& place) = 0;
    virtual void Unbind() = 0;

    virtual void UpdateResources(GraphicsContext& context) = 0;
    virtual void WriteSceneBinding(GraphicsContext& context, ResourceBinding& binding) const = 0;
    virtual void Draw(GraphicsContext& context, CommandBuffer& commandBuffer, std::uint32_t frameIndex, const Objects::Camera& camera) = 0;

    [[nodiscard]] virtual Math::Color3 GetSkyTint() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
