#include "Lvs/Studio/Core/ViewportManager.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/Viewport.hpp"
#include "Lvs/Engine/Core/Window.hpp"

namespace Lvs::Studio::Core {

ViewportManager::ViewportManager(Engine::Core::Window& window, const Engine::EngineContextPtr& context) {
    viewport_ = new Engine::Core::Viewport(context, &window);
    BindSettings();
    viewport_->hide();
    window.AddWidget(viewport_);
}

ViewportManager::~ViewportManager() = default;

Engine::Core::Viewport* ViewportManager::GetViewport() const {
    return viewport_;
}

void ViewportManager::BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place) const {
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->BindToPlace(place);
}

void ViewportManager::Unbind() const {
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->Unbind();
}

void ViewportManager::Show() const {
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->show();
}

void ViewportManager::Hide() const {
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->hide();
}

void ViewportManager::BindSettings() {
    if (viewport_ == nullptr) {
        return;
    }

    settingsConnections_.push_back(Settings::Changed("BaseCameraSpeed", [this](const QVariant& value) {
        if (viewport_ != nullptr) {
            viewport_->SetCameraSpeed(value.toDouble());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("ShiftCameraSpeed", [this](const QVariant& value) {
        if (viewport_ != nullptr) {
            viewport_->SetCameraShiftSpeed(value.toDouble());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("GizmoAlwaysOnTop", [this](const QVariant& value) {
        if (viewport_ != nullptr) {
            viewport_->SetGizmoAlwaysOnTop(value.toBool());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("GizmoIgnoreDiffuseSpecular", [this](const QVariant& value) {
        if (viewport_ != nullptr) {
            viewport_->SetGizmoIgnoreDiffuseSpecular(value.toBool());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("GizmoAlignByMagnitude", [this](const QVariant& value) {
        if (viewport_ != nullptr) {
            viewport_->SetGizmoAlignByMagnitude(value.toBool());
        }
    }, true));
}

} // namespace Lvs::Studio::Core
