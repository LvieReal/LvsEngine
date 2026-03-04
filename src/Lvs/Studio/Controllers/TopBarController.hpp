#pragma once

#include <memory>

class QAction;
class QDockWidget;
class QMenu;
class QToolBar;
class QToolButton;

#include <QList>
#include <QPair>
#include <QString>

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Studio::Core {
class DockManager;
}

namespace Lvs::Engine::DataModel {
class PlaceManager;
}

namespace Lvs::Studio::Widgets {
class AboutStudioDialog;
}

namespace Lvs::Studio::Widgets::Settings {
class SettingsWidget;
}

namespace Lvs::Studio::Controllers {

class TopBarController final {
public:
    TopBarController(
        Engine::Core::Window& window,
        Core::DockManager& dockManager,
        Engine::DataModel::PlaceManager& placeManager
    );
    ~TopBarController();
    void Build();

private:
    void BuildFileMenu();
    void BuildViewMenu();
    void RefreshFileActions();
    void RefreshViewActions();
    bool SaveCurrentPlaceWithDialog() const;
    bool CloseCurrentPlaceIfAllowed();
    bool CanCloseCurrentPlace();
    QString PromptSavePath() const;
    void SaveCurrentPlaceToPath(const QString& path) const;

    Engine::Core::Window& window_;
    Core::DockManager& dockManager_;
    Engine::DataModel::PlaceManager& placeManager_;
    QToolBar* topBar_{nullptr};
    QToolButton* fileButton_{nullptr};
    QToolButton* viewButton_{nullptr};
    QMenu* fileMenu_{nullptr};
    QMenu* viewMenu_{nullptr};
    QAction* newAction_{nullptr};
    QAction* openAction_{nullptr};
    QAction* saveAction_{nullptr};
    QAction* closeAction_{nullptr};
    QList<QPair<QDockWidget*, QAction*>> viewActions_;
    std::unique_ptr<Widgets::AboutStudioDialog> aboutDialog_;
    std::unique_ptr<Widgets::Settings::SettingsWidget> settingsWidget_;
};

} // namespace Lvs::Studio::Controllers
