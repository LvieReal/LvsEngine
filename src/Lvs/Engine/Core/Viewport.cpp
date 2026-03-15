#include "Lvs/Engine/Core/Viewport.hpp"

#include "Lvs/Engine/Core/CameraController.hpp"
#include "Lvs/Engine/Core/Cursor.hpp"
#include "Lvs/Engine/Core/ViewportToolLayer.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include <Qt>

namespace Lvs::Engine::Core {

Viewport::Viewport(const EngineContextPtr& context, QWidget* parent)
    : QWidget(parent),
      context_(context) {
    Cursor::SetCustomCursor(this);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

Viewport::~Viewport() = default;

void Viewport::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
    workspace_ = place != nullptr
        ? std::dynamic_pointer_cast<DataModel::Workspace>(place->FindService("Workspace"))
        : nullptr;
    cameraController_.reset();
    if (workspace_ != nullptr) {
        const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
        if (camera != nullptr) {
            cameraController_ = std::make_unique<CameraController>(camera);
            cameraController_->SetSpeed(cameraSpeed_);
            cameraController_->SetShiftSpeed(cameraShiftSpeed_);
        }
    }

    if (toolLayer_ != nullptr) {
        toolLayer_->BindToPlace(place);
    }

    if (context_ != nullptr && context_->RenderContext != nullptr) {
        context_->RenderContext->BindToPlace(place);
    }
    update();
}

void Viewport::Unbind() {
    place_.reset();
    workspace_.reset();
    cameraController_.reset();
    pressedKeys_.clear();
    pressedScanCodes_.clear();
    rightMouseDown_ = false;
    rightMousePanned_ = false;

    if (toolLayer_ != nullptr) {
        toolLayer_->Unbind();
    }

    if (context_ != nullptr && context_->RenderContext != nullptr) {
        context_->RenderContext->Unbind();
    }
}

void Viewport::SetCameraSpeed(const double speed) {
    cameraSpeed_ = speed;
    if (cameraController_ != nullptr) {
        cameraController_->SetSpeed(speed);
    }
}

void Viewport::SetCameraShiftSpeed(const double speed) {
    cameraShiftSpeed_ = speed;
    if (cameraController_ != nullptr) {
        cameraController_->SetShiftSpeed(speed);
    }
}

void Viewport::SetRenderingApiPreference(const Rendering::RenderApi api) {
    RecreateRenderContext(api);
}

void Viewport::SetToolLayer(std::unique_ptr<ViewportToolLayer> layer) {
    if (toolLayer_ != nullptr) {
        toolLayer_->Unbind();
    }
    toolLayer_ = std::move(layer);
    if (toolLayer_ != nullptr) {
        if (const auto place = place_.lock(); place != nullptr) {
            toolLayer_->BindToPlace(place);
        }
    }
}

ViewportToolLayer* Viewport::GetToolLayer() const {
    return toolLayer_.get();
}

bool Viewport::WasRightMousePanned() const {
    return rightMousePanned_;
}

} // namespace Lvs::Engine::Core
