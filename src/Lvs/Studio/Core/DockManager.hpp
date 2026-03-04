#pragma once

#include <QHash>
#include <QList>
#include <QSet>

#include <memory>

class QDockWidget;

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Studio::Widgets::Explorer {
class ExplorerDock;
}

namespace Lvs::Studio::Widgets::Properties {
class PropertiesDock;
}

namespace Lvs::Studio::Widgets::Output {
class OutputDock;
}

namespace Lvs::Studio::Core {

class DockManager final {
public:
    explicit DockManager(Engine::Core::Window& window);

    void Build();
    void BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place);
    void Unbind();
    void SaveState() const;
    void CachePlaceRequiredDockVisibility();
    void ApplyPlaceRequiredDockVisibility();
    void HidePlaceRequiredDocks();

    QList<QDockWidget*> GetDockableWidgets() const;
    bool DockRequiresOpenPlace(const QDockWidget* dock) const;

private:
    bool RestoreDockState() const;
    void ApplyDefaultSizing() const;
    void ApplyDefaultHiddenState() const;

    Engine::Core::Window& window_;
    Widgets::Explorer::ExplorerDock* explorer_{nullptr};
    Widgets::Properties::PropertiesDock* properties_{nullptr};
    Widgets::Output::OutputDock* output_{nullptr};
    QList<QDockWidget*> placeRequiredDocks_;
    QHash<QString, bool> cachedPlaceRequiredVisibility_;
    bool isStateSaveSuppressed_{false};
    bool hasOpenPlace_{false};
    QSet<QString> hiddenByDefaultDockObjectNames_{"Dock.Output"};
};

} // namespace Lvs::Studio::Core
