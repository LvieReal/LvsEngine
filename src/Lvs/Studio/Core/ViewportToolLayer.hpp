#pragma once

#include "Lvs/Engine/Rendering/Common/Image3DPrimitive.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <memory>
#include <optional>
#include <vector>

class QMouseEvent;

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Core {

class ViewportToolLayer {
public:
    virtual ~ViewportToolLayer() = default;

    virtual void BindToPlace(const std::shared_ptr<DataModel::Place>& place) = 0;
    virtual void Unbind() = 0;

    virtual void OnFrame(double deltaSeconds, const std::optional<Utils::Ray>& cursorRay) = 0;
    virtual void OnMousePress(QMouseEvent* event, const std::optional<Utils::Ray>& ray) = 0;
    virtual void OnMouseRelease(QMouseEvent* event) = 0;
    virtual void OnMouseMove(QMouseEvent* event, const std::optional<Utils::Ray>& ray) = 0;
    virtual void OnFocusOut() = 0;

    virtual void AppendOverlay(std::vector<Rendering::Common::OverlayPrimitive>& overlay) = 0;
    virtual void AppendImage3D(std::vector<Rendering::Common::Image3DPrimitive>& images) = 0;
};

} // namespace Lvs::Engine::Core
