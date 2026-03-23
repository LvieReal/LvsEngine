#include "Lvs/Studio/Core/DockManager.hpp"

#include "Lvs/Studio/Core/Window.hpp"
#include "Lvs/Studio/Core/Settings.hpp"
#include "Lvs/Studio/Widgets/Explorer/ExplorerDock.hpp"
#include "Lvs/Studio/Widgets/Output/OutputDock.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertiesDock.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDockWidget>
#include <QObject>
#include <Qt>

namespace Lvs::Studio::Core {

namespace {
constexpr int STATE_VERSION = 1;
}

DockManager::DockManager(Engine::Core::Window& window)
    : window_(window) {
}

void DockManager::Build() {
    window_.setDockNestingEnabled(true);
    window_.setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    window_.setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    explorer_ = new Widgets::Explorer::ExplorerDock(&window_);
    properties_ = new Widgets::Properties::PropertiesDock(&window_);
    output_ = new Widgets::Output::OutputDock(&window_);

    explorer_->setObjectName("Dock.Explorer");
    properties_->setObjectName("Dock.Properties");
    output_->setObjectName("Dock.Output");

    placeRequiredDocks_ = {explorer_, properties_, output_};

    window_.addDockWidget(Qt::RightDockWidgetArea, explorer_);
    window_.addDockWidget(Qt::RightDockWidgetArea, properties_);
    window_.splitDockWidget(explorer_, properties_, Qt::Vertical);
    window_.addDockWidget(Qt::BottomDockWidgetArea, output_);

    if (!RestoreDockState()) {
        ApplyDefaultSizing();
        ApplyDefaultHiddenState();
    }

    for (QDockWidget* dock : GetDockableWidgets()) {
        QObject::connect(dock, &QDockWidget::visibilityChanged, &window_, [this](bool) { SaveState(); });
        QObject::connect(dock, &QDockWidget::dockLocationChanged, &window_, [this](Qt::DockWidgetArea) { SaveState(); });
        QObject::connect(dock, &QDockWidget::topLevelChanged, &window_, [this](bool) { SaveState(); });
    }

    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance()); app != nullptr) {
        QObject::connect(app, &QApplication::aboutToQuit, &window_, [this]() { SaveState(); });
    }
}

void DockManager::BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place) {
    hasOpenPlace_ = true;
    if (explorer_ != nullptr) {
        explorer_->BindToPlace(place);
    }
    if (properties_ != nullptr) {
        properties_->BindToPlace(place);
    }
    if (output_ != nullptr) {
        output_->BindToPlace(place);
    }
}

void DockManager::Unbind() {
    hasOpenPlace_ = false;
    if (explorer_ != nullptr) {
        explorer_->Unbind();
    }
    if (properties_ != nullptr) {
        properties_->Unbind();
    }
    if (output_ != nullptr) {
        output_->Unbind();
    }
}

void DockManager::SaveState() const {
    if (isStateSaveSuppressed_) {
        return;
    }
    const QByteArray state = window_.saveState(STATE_VERSION);
    const QString encoded = QString::fromLatin1(state.toBase64());
    Core::Settings::Set("DockLayoutState", encoded);
}

void DockManager::CachePlaceRequiredDockVisibility() {
    if (!hasOpenPlace_) {
        return;
    }
    SaveState();
    cachedPlaceRequiredVisibility_.clear();
    for (QDockWidget* dock : GetDockableWidgets()) {
        if (!DockRequiresOpenPlace(dock)) {
            continue;
        }
        cachedPlaceRequiredVisibility_.insert(dock->objectName(), !dock->isHidden());
    }
}

void DockManager::ApplyPlaceRequiredDockVisibility() {
    isStateSaveSuppressed_ = true;
    for (QDockWidget* dock : GetDockableWidgets()) {
        if (!DockRequiresOpenPlace(dock)) {
            continue;
        }
        const bool isVisible = cachedPlaceRequiredVisibility_.value(
            dock->objectName(),
            !hiddenByDefaultDockObjectNames_.contains(dock->objectName())
        );
        dock->setVisible(isVisible);
    }
    isStateSaveSuppressed_ = false;
}

void DockManager::HidePlaceRequiredDocks() {
    isStateSaveSuppressed_ = true;
    for (QDockWidget* dock : GetDockableWidgets()) {
        if (!DockRequiresOpenPlace(dock)) {
            continue;
        }
        if (dock->isFloating()) {
            dock->setFloating(false);
        }
        dock->hide();
    }
    isStateSaveSuppressed_ = false;
}

QList<QDockWidget*> DockManager::GetDockableWidgets() const {
    return {explorer_, properties_, output_};
}

bool DockManager::DockRequiresOpenPlace(const QDockWidget* dock) const {
    return placeRequiredDocks_.contains(const_cast<QDockWidget*>(dock));
}

bool DockManager::RestoreDockState() const {
    const QString encodedState = Core::Settings::Get("DockLayoutState").toString();
    if (encodedState.isEmpty()) {
        return false;
    }
    const QByteArray decoded = QByteArray::fromBase64(encodedState.toLatin1());
    return window_.restoreState(decoded, STATE_VERSION);
}

void DockManager::ApplyDefaultSizing() const {
    const int totalWidth = window_.width();
    const int defaultWidth = static_cast<int>(totalWidth * 0.125);
    window_.resizeDocks({explorer_, properties_}, {defaultWidth, defaultWidth}, Qt::Horizontal);

    const int totalHeight = window_.height();
    const int explorerHeight = static_cast<int>(totalHeight * 0.40);
    const int propertiesHeight = static_cast<int>(totalHeight * 0.60);
    window_.resizeDocks({explorer_, properties_}, {explorerHeight, propertiesHeight}, Qt::Vertical);
}

void DockManager::ApplyDefaultHiddenState() const {
    for (QDockWidget* dock : GetDockableWidgets()) {
        if (hiddenByDefaultDockObjectNames_.contains(dock->objectName())) {
            dock->hide();
        } else {
            dock->show();
        }
    }
}

} // namespace Lvs::Studio::Core
