#include "Lvs/Studio/Core/ViewportManager.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/Viewport.hpp"
#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/DataModel/Services/QualitySettings.hpp"
#include "Lvs/Engine/Enums/EnumMetadata.hpp"
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
	            const int typeId = QMetaType::fromType<Engine::Rendering::RenderApi>().id();
	            QVariant api = Engine::Enums::Metadata::CoerceVariant(typeId, value);
	            if (!api.isValid()) {
	                api = QVariant::fromValue(Engine::Rendering::RenderApi::Auto);
	            }
	            viewport_->SetRenderingApiPreference(api.value<Engine::Rendering::RenderApi>());
	        }
	    }, true));
    settingsConnections_.push_back(Settings::Changed("MSAA", [this](const QVariant& value) {
        ApplyMsaaSetting(value);
    }, true));
    settingsConnections_.push_back(Settings::Changed("SurfaceMipmapping", [this](const QVariant& value) {
        ApplySurfaceMipmappingSetting(value);
    }, true));
    settingsConnections_.push_back(Settings::Changed("RefreshShaders", [this](const QVariant&) {
        if (viewport_ != nullptr) {
            viewport_->RefreshShaders();
        }
    }, false));
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

    const int typeId = QMetaType::fromType<Engine::Enums::MSAA>().id();
    QVariant msaa = Engine::Enums::Metadata::CoerceVariant(typeId, value);
    if (!msaa.isValid()) {
        msaa = QVariant::fromValue(Engine::Enums::MSAA::Off);
    }
    qualitySettings->SetProperty("MSAA", msaa);
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

    const int typeId = QMetaType::fromType<Engine::Enums::SurfaceMipmapping>().id();
    QVariant mipmapping = Engine::Enums::Metadata::CoerceVariant(typeId, value);
    if (!mipmapping.isValid()) {
        mipmapping = QVariant::fromValue(Engine::Enums::SurfaceMipmapping::On);
    }
    qualitySettings->SetProperty("SurfaceMipmapping", mipmapping);
}

} // namespace Lvs::Studio::Core
