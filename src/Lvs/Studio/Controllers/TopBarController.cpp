#include "Lvs/Studio/Controllers/TopBarController.hpp"

#include "Lvs/Engine/Core/RegularError.hpp"
#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Studio/Core/DockManager.hpp"
#include "Lvs/Studio/Widgets/AboutStudioDialog.hpp"
#include "Lvs/Studio/Widgets/Settings/SettingsWidget.hpp"

#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSize>
#include <QToolBar>
#include <QToolButton>
#include <Qt>

#include <exception>

namespace Lvs::Studio::Controllers {

TopBarController::TopBarController(
    Engine::Core::Window& window,
    Core::DockManager& dockManager,
    Engine::DataModel::PlaceManager& placeManager
)
    : window_(window),
      dockManager_(dockManager),
      placeManager_(placeManager) {
}

TopBarController::~TopBarController() = default;

void TopBarController::Build() {
    topBar_ = new QToolBar("TopBar", &window_);
    topBar_->setObjectName("TopBar");
    topBar_->setMovable(false);
    topBar_->setFloatable(false);
    topBar_->setIconSize(QSize(14, 14));

    fileButton_ = new QToolButton(topBar_);
    fileButton_->setObjectName("TopBarAction");
    fileButton_->setText("File");
    fileButton_->setPopupMode(QToolButton::InstantPopup);
    topBar_->addWidget(fileButton_);

    viewButton_ = new QToolButton(topBar_);
    viewButton_->setObjectName("TopBarAction");
    viewButton_->setText("View");
    viewButton_->setPopupMode(QToolButton::InstantPopup);
    topBar_->addWidget(viewButton_);

    BuildFileMenu();
    BuildViewMenu();

    window_.addToolBar(Qt::TopToolBarArea, topBar_);
}

void TopBarController::BuildFileMenu() {
    fileMenu_ = new QMenu(fileButton_);
    fileButton_->setMenu(fileMenu_);

    settingsWidget_ = std::make_unique<Widgets::Settings::SettingsWidget>(&window_);
    aboutDialog_ = std::make_unique<Widgets::AboutStudioDialog>(&window_);

    newAction_ = fileMenu_->addAction("New");
    openAction_ = fileMenu_->addAction("Open from File");
    saveAction_ = fileMenu_->addAction("Save to File");

    QObject::connect(newAction_, &QAction::triggered, &window_, [this]() {
        if (!CloseCurrentPlaceIfAllowed()) {
            return;
        }
        try {
            window_.ShowBusy("Creating place...");
            placeManager_.NewPlace();
            window_.HideBusy("Ready");
            RefreshFileActions();
            RefreshViewActions();
        } catch (const std::exception& ex) {
            window_.HideBusy();
            Engine::Core::RegularError::ShowErrorFromException(ex);
        }
    });

    QObject::connect(openAction_, &QAction::triggered, &window_, [this]() {
        const QString defaultPath = placeManager_.GetCurrentPlace() != nullptr
            ? placeManager_.GetCurrentPlace()->GetFilePath()
            : QString{};
        const QString selectedPath = QFileDialog::getOpenFileName(
            &window_,
            "Open Place from File",
            defaultPath,
            "Lvs Place Files (*.lvsx);;XML Files (*.xml);;All Files (*)"
        );
        if (selectedPath.isEmpty()) {
            return;
        }
        if (placeManager_.GetCurrentPlace() != nullptr && !CloseCurrentPlaceIfAllowed()) {
            return;
        }

        try {
            window_.ShowBusy("Opening place...");
            placeManager_.OpenPlaceFromFile(selectedPath);
            window_.HideBusy("Ready");
            RefreshFileActions();
            RefreshViewActions();
        } catch (const std::exception& ex) {
            window_.HideBusy();
            Engine::Core::RegularError::ShowErrorFromException(ex);
        }
    });

    QObject::connect(saveAction_, &QAction::triggered, &window_, [this]() {
        try {
            static_cast<void>(SaveCurrentPlaceWithDialog());
            RefreshFileActions();
        } catch (const std::exception& ex) {
            window_.HideBusy();
            Engine::Core::RegularError::ShowErrorFromException(ex);
        }
    });

    fileMenu_->addSeparator();
    auto* settingsAction = fileMenu_->addAction("Studio Settings");
    auto* aboutAction = fileMenu_->addAction("About Studio");
    fileMenu_->addSeparator();
    closeAction_ = fileMenu_->addAction("Close");

    QObject::connect(settingsAction, &QAction::triggered, &window_, [this]() {
        settingsWidget_->show();
        settingsWidget_->raise();
        settingsWidget_->activateWindow();
    });

    QObject::connect(aboutAction, &QAction::triggered, &window_, [this]() {
        aboutDialog_->show();
        aboutDialog_->raise();
        aboutDialog_->activateWindow();
    });

    QObject::connect(closeAction_, &QAction::triggered, &window_, [this]() {
        static_cast<void>(CloseCurrentPlaceIfAllowed());
        RefreshFileActions();
        RefreshViewActions();
    });

    window_.SetBeforeCloseHandler([this]() {
        return CloseCurrentPlaceIfAllowed();
    });

    placeManager_.PlaceOpened.Connect([this](const std::shared_ptr<Engine::DataModel::Place>&) {
        RefreshFileActions();
        RefreshViewActions();
    });
    placeManager_.PlaceClosed.Connect([this](const std::shared_ptr<Engine::DataModel::Place>&) {
        RefreshFileActions();
        RefreshViewActions();
    });
    RefreshFileActions();
}

void TopBarController::BuildViewMenu() {
    viewMenu_ = new QMenu(viewButton_);
    viewButton_->setMenu(viewMenu_);

    for (QDockWidget* dock : dockManager_.GetDockableWidgets()) {
        QAction* action = dock->toggleViewAction();
        viewMenu_->addAction(action);
        viewActions_.push_back({dock, action});
    }
    RefreshViewActions();
}

void TopBarController::RefreshFileActions() {
    const bool hasPlace = placeManager_.GetCurrentPlace() != nullptr;
    if (newAction_ != nullptr) {
        newAction_->setEnabled(true);
    }
    if (openAction_ != nullptr) {
        openAction_->setEnabled(true);
    }
    if (saveAction_ != nullptr) {
        saveAction_->setEnabled(hasPlace);
    }
    if (closeAction_ != nullptr) {
        closeAction_->setVisible(hasPlace);
        closeAction_->setEnabled(hasPlace);
    }
}

void TopBarController::RefreshViewActions() {
    const bool hasPlace = placeManager_.GetCurrentPlace() != nullptr;
    for (const auto& [dock, action] : viewActions_) {
        if (dockManager_.DockRequiresOpenPlace(dock)) {
            action->setEnabled(hasPlace);
        } else {
            action->setEnabled(true);
        }
    }
}

QString TopBarController::PromptSavePath() const {
    const auto currentPlace = placeManager_.GetCurrentPlace();
    if (currentPlace == nullptr) {
        return {};
    }

    QString defaultPath = currentPlace->GetFilePath();
    if (defaultPath.isEmpty()) {
        defaultPath = "untitled.lvsx";
    }
    QString selectedPath = QFileDialog::getSaveFileName(
        &window_,
        "Save Place to File",
        defaultPath,
        "Lvs Place Files (*.lvsx);;XML Files (*.xml);;All Files (*)"
    );
    if (selectedPath.isEmpty()) {
        return {};
    }

    QFileInfo info(selectedPath);
    if (info.suffix().compare("lvsx", Qt::CaseInsensitive) != 0) {
        selectedPath += ".lvsx";
    }
    return selectedPath;
}

void TopBarController::SaveCurrentPlaceToPath(const QString& path) const {
    if (path.isEmpty()) {
        return;
    }

    window_.ShowBusy("Saving place...");
    try {
        placeManager_.SaveCurrentPlaceToFile(path);
        window_.HideBusy("Ready");
    } catch (...) {
        window_.HideBusy();
        throw;
    }
}

bool TopBarController::SaveCurrentPlaceWithDialog() const {
    const auto currentPlace = placeManager_.GetCurrentPlace();
    if (currentPlace == nullptr) {
        return false;
    }

    const QString outputPath = PromptSavePath();
    if (outputPath.isEmpty()) {
        return false;
    }

    SaveCurrentPlaceToPath(outputPath);
    return true;
}

bool TopBarController::CanCloseCurrentPlace() {
    const auto currentPlace = placeManager_.GetCurrentPlace();
    if (currentPlace == nullptr || !currentPlace->IsDirty()) {
        return true;
    }

    QMessageBox box(&window_);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle("Unsaved Changes");
    box.setText("This place has unsaved changes.");
    box.setInformativeText("Do you want to save your changes before closing?");
    QPushButton* saveButton = box.addButton("Save", QMessageBox::AcceptRole);
    QPushButton* discardButton = box.addButton("Don't Save", QMessageBox::DestructiveRole);
    QPushButton* cancelButton = box.addButton("Cancel", QMessageBox::RejectRole);
    box.setDefaultButton(saveButton);
    box.exec();

    if (box.clickedButton() == cancelButton) {
        return false;
    }
    if (box.clickedButton() == saveButton) {
        try {
            return SaveCurrentPlaceWithDialog();
        } catch (const std::exception& ex) {
            Engine::Core::RegularError::ShowErrorFromException(ex);
            return false;
        }
    }
    return box.clickedButton() == discardButton;
}

bool TopBarController::CloseCurrentPlaceIfAllowed() {
    if (!CanCloseCurrentPlace()) {
        return false;
    }

    if (placeManager_.GetCurrentPlace() == nullptr) {
        return true;
    }

    try {
        window_.ShowBusy("Closing place...");
        placeManager_.ClosePlace();
        window_.HideBusy("Ready");
        return true;
    } catch (const std::exception& ex) {
        window_.HideBusy();
        Engine::Core::RegularError::ShowErrorFromException(ex);
        return false;
    }
}

} // namespace Lvs::Studio::Controllers
