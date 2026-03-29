#pragma once

#include "Lvs/Engine/DataModel/Place.hpp"

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
class StudioQuickActions;
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
    void SetQuickActions(Core::StudioQuickActions* quickActions);

private:
    void BuildFileMenu();
    void BuildEditMenu();
    void BuildViewMenu();
    void BuildToolsMenu();
    void RefreshFileActions();
    void RefreshEditActions();
    void RefreshViewActions();
    bool SaveCurrentPlaceWithDialog() const;
    bool SaveCurrentPlaceAsTomlWithDialog() const;
    bool CloseCurrentPlaceIfAllowed();
    bool CanCloseCurrentPlace();
    QString PromptSavePath() const;
    QString PromptSaveTomlPath() const;
    void SaveCurrentPlaceToPath(const QString& path) const;
    void SaveCurrentPlaceToPathAs(const QString& path, Engine::DataModel::Place::FileFormat format) const;

    Engine::Core::Window& window_;
    Core::DockManager& dockManager_;
    Engine::DataModel::PlaceManager& placeManager_;
    QToolBar* topBar_{nullptr};
    QToolButton* fileButton_{nullptr};
    QToolButton* editButton_{nullptr};
    QToolButton* viewButton_{nullptr};
    QToolButton* toolsButton_{nullptr};
    QMenu* fileMenu_{nullptr};
    QMenu* editMenu_{nullptr};
    QMenu* viewMenu_{nullptr};
    QMenu* toolsMenu_{nullptr};
    QAction* newAction_{nullptr};
    QAction* openAction_{nullptr};
    QAction* saveAction_{nullptr};
    QAction* saveAsTomlAction_{nullptr};
    QAction* closeAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* cutAction_{nullptr};
    QAction* copyAction_{nullptr};
    QAction* pasteAction_{nullptr};
    QAction* deleteAction_{nullptr};
    QAction* selectAllAction_{nullptr};
    QAction* duplicateAction_{nullptr};
    QList<QPair<QDockWidget*, QAction*>> viewActions_;
    std::unique_ptr<Widgets::AboutStudioDialog> aboutDialog_;
    std::unique_ptr<Widgets::Settings::SettingsWidget> settingsWidget_;
    Core::StudioQuickActions* quickActions_{nullptr};
};

} // namespace Lvs::Studio::Controllers
