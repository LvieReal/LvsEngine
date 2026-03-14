#include "Lvs/Studio/Core/ViewportManager.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/Viewport.hpp"
#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/DataModel/QualitySettings.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Studio/Core/StudioViewportToolLayer.hpp"

namespace Lvs::Studio::Core {

ViewportManager::ViewportManager(Engine::Core::Window& window, const Engine::EngineContextPtr& context) {
    viewport_ = new Engine::Core::Viewport(context, &window);
    auto toolLayer = std::make_unique<StudioViewportToolLayer>(*viewport_, context);
    toolLayer_ = toolLayer.get();
    viewport_->SetToolLayer(std::move(toolLayer));
    BindSettings();
    viewport_->hide();
    window.AddWidget(viewport_);
}

ViewportManager::~ViewportManager() = default;

Engine::Core::Viewport* ViewportManager::GetViewport() const {
    return viewport_;
}

void ViewportManager::BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place) {
    place_ = place;
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->BindToPlace(place);
    ApplyMsaaSetting(Settings::Get("MSAA"));
    ApplySurfaceMipmappingSetting(Settings::Get("SurfaceMipmapping"));
}

void ViewportManager::Unbind() {
    place_.reset();
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
        if (toolLayer_ != nullptr) {
            toolLayer_->SetGizmoAlwaysOnTop(value.toBool());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("GizmoIgnoreDiffuseSpecular", [this](const QVariant& value) {
        if (toolLayer_ != nullptr) {
            toolLayer_->SetGizmoIgnoreDiffuseSpecular(value.toBool());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("GizmoAlignByMagnitude", [this](const QVariant& value) {
        if (toolLayer_ != nullptr) {
            toolLayer_->SetGizmoAlignByMagnitude(value.toBool());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("TransformSnapIncrement", [this](const QVariant& value) {
        if (toolLayer_ != nullptr) {
            toolLayer_->SetSnapIncrement(value.toDouble());
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("RenderingApi", [this](const QVariant& value) {
        if (viewport_ != nullptr) {
            viewport_->SetRenderingApiPreference(
                Engine::Rendering::ParseRenderApi(value.toString().toStdString())
            );
        }
    }, true));
    settingsConnections_.push_back(Settings::Changed("MSAA", [this](const QVariant& value) {
        ApplyMsaaSetting(value);
    }, true));
    settingsConnections_.push_back(Settings::Changed("SurfaceMipmapping", [this](const QVariant& value) {
        ApplySurfaceMipmappingSetting(value);
    }, true));
}

void ViewportManager::ApplyMsaaSetting(const QVariant& value) const {
    const auto place = place_.lock();
    if (place == nullptr) {
        return;
    }
    const auto qualitySettings = std::dynamic_pointer_cast<Engine::DataModel::QualitySettings>(
        place->FindService("QualitySettings")
    );
    if (qualitySettings == nullptr) {
        return;
    }

    Engine::Enums::MSAA msaa = Engine::Enums::MSAA::Off;
    const QString text = value.toString().trimmed().toLower();
    if (text == "2x" || text == "2") {
        msaa = Engine::Enums::MSAA::X2;
    } else if (text == "4x" || text == "4") {
        msaa = Engine::Enums::MSAA::X4;
    } else if (text == "8x" || text == "8") {
        msaa = Engine::Enums::MSAA::X8;
    }
    qualitySettings->SetProperty("MSAA", QVariant::fromValue(msaa));
}

void ViewportManager::ApplySurfaceMipmappingSetting(const QVariant& value) const {
    const auto place = place_.lock();
    if (place == nullptr) {
        return;
    }
    const auto qualitySettings = std::dynamic_pointer_cast<Engine::DataModel::QualitySettings>(
        place->FindService("QualitySettings")
    );
    if (qualitySettings == nullptr) {
        return;
    }

    Engine::Enums::SurfaceMipmapping mipmapping = Engine::Enums::SurfaceMipmapping::On;
    const QString text = value.toString().trimmed().toLower();
    if (text == "off" || text == "0" || text == "false") {
        mipmapping = Engine::Enums::SurfaceMipmapping::Off;
    }
    qualitySettings->SetProperty("SurfaceMipmapping", QVariant::fromValue(mipmapping));
}

} // namespace Lvs::Studio::Core
