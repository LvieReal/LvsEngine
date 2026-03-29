#include "Lvs/Studio/Controllers/TopBarController.hpp"

#include "Lvs/Qt/QtBridge.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Services/Selection.hpp"
#include "Lvs/Studio/Core/RegularError.hpp"
#include "Lvs/Studio/Core/Window.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Studio/Core/DockManager.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"
#include "Lvs/Studio/Core/Settings.hpp"
#include "Lvs/Studio/Core/StudioQuickActions.hpp"
#include "Lvs/Studio/Core/StudioShortcutManager.hpp"
#include "Lvs/Studio/Widgets/AboutStudioDialog.hpp"
#include "Lvs/Studio/Widgets/Settings/SettingsWidget.hpp"
#include "Lvs/Engine/Core/PlaceFileUtils.hpp"

#include <QAction>
#include <QCheckBox>
#include <QDockWidget>
#include <QFileDialog>
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

void TopBarController::SetQuickActions(Core::StudioQuickActions* quickActions) {
    quickActions_ = quickActions;
    RefreshEditActions();
}

void TopBarController::Build() {
    topBar_ = new QToolBar("TopBar", &window_);
    topBar_->setObjectName("TopBar");
    topBar_->setMovable(true);
    topBar_->setFloatable(true);
    topBar_->setAllowedAreas(Qt::AllToolBarAreas);
    topBar_->setIconSize(QSize(14, 14));

    fileButton_ = new QToolButton(topBar_);
    fileButton_->setObjectName("TopBarAction");
    fileButton_->setText("File");
    fileButton_->setPopupMode(QToolButton::InstantPopup);
    topBar_->addWidget(fileButton_);

    editButton_ = new QToolButton(topBar_);
    editButton_->setObjectName("TopBarAction");
    editButton_->setText("Edit");
    editButton_->setPopupMode(QToolButton::InstantPopup);
    topBar_->addWidget(editButton_);

    viewButton_ = new QToolButton(topBar_);
    viewButton_->setObjectName("TopBarAction");
    viewButton_->setText("View");
    viewButton_->setPopupMode(QToolButton::InstantPopup);
    topBar_->addWidget(viewButton_);

    toolsButton_ = new QToolButton(topBar_);
    toolsButton_->setObjectName("TopBarAction");
    toolsButton_->setText("Tools");
    toolsButton_->setPopupMode(QToolButton::InstantPopup);
    topBar_->addWidget(toolsButton_);

    BuildFileMenu();
    BuildEditMenu();
    BuildViewMenu();
    BuildToolsMenu();

    window_.addToolBar(Qt::TopToolBarArea, topBar_);
}

void TopBarController::BuildFileMenu() {
    fileMenu_ = new QMenu(fileButton_);
    fileButton_->setMenu(fileMenu_);

    newAction_ = fileMenu_->addAction("New");
    openAction_ = fileMenu_->addAction("Open from File");
    saveAction_ = fileMenu_->addAction("Save to File");
    saveAsTomlAction_ = fileMenu_->addAction("Save to File as TOML");

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
            RefreshEditActions();
        } catch (const std::exception& ex) {
            window_.HideBusy();
            Engine::Core::RegularError::ShowErrorFromException(ex);
        }
    });

    QObject::connect(openAction_, &QAction::triggered, &window_, [this]() {
        const QString defaultPath = placeManager_.GetCurrentPlace() != nullptr
            ? Engine::Core::QtBridge::ToQString(placeManager_.GetCurrentPlace()->GetFilePath())
            : QString{};
        const QString selectedPath = QFileDialog::getOpenFileName(
            &window_,
            "Open Place from File",
            defaultPath,
            Engine::Core::QtBridge::ToQString(Engine::Core::PlaceFileUtils::FileDialogOpenFilter())
        );
        if (selectedPath.isEmpty()) {
            return;
        }
        if (placeManager_.GetCurrentPlace() != nullptr && !CloseCurrentPlaceIfAllowed()) {
            return;
        }

        try {
            window_.ShowBusy("Opening place...");
            placeManager_.OpenPlaceFromFile(Engine::Core::QtBridge::ToStdString(selectedPath));
            window_.HideBusy("Ready");
            RefreshFileActions();
            RefreshViewActions();
            RefreshEditActions();

            const auto place = placeManager_.GetCurrentPlace();
            if (place != nullptr &&
                place->GetLoadedFileFormat() == Engine::DataModel::Place::FileFormat::Xml &&
                Core::Settings::Get("AskMigrateXmlPlaceToToml").toBool()) {

                const QString suffix = QFileInfo(selectedPath).suffix();
                if (suffix.compare("lvsx", Qt::CaseInsensitive) == 0) {
                    QMessageBox box(&window_);
                    box.setWindowTitle("Migrate Place to TOML");
                    box.setIcon(QMessageBox::Question);
                    box.setText("This place file uses legacy XML format.");
                    box.setInformativeText("Convert it to TOML now? (Recommended)");
                    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    box.setDefaultButton(QMessageBox::Yes);

                    QCheckBox dontAsk("Don't ask again");
                    box.setCheckBox(&dontAsk);

                    const int res = box.exec();
                    if (dontAsk.isChecked()) {
                        Core::Settings::Set("AskMigrateXmlPlaceToToml", false);
                        Core::Settings::Save();
                    }
                    if (res == QMessageBox::Yes) {
                        window_.ShowBusy("Migrating place...");
                        place->SetPreferredSaveFormat(Engine::DataModel::Place::FileFormat::Toml);
                        placeManager_.SaveCurrentPlaceToFile(place->GetFilePath());
                        window_.HideBusy("Ready");
                    }
                }
            }
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

    QObject::connect(saveAsTomlAction_, &QAction::triggered, &window_, [this]() {
        try {
            static_cast<void>(SaveCurrentPlaceAsTomlWithDialog());
            RefreshFileActions();
        } catch (const std::exception& ex) {
            window_.HideBusy();
            Engine::Core::RegularError::ShowErrorFromException(ex);
        }
    });

    fileMenu_->addSeparator();
    closeAction_ = fileMenu_->addAction("Close");

    QObject::connect(closeAction_, &QAction::triggered, &window_, [this]() {
        static_cast<void>(CloseCurrentPlaceIfAllowed());
        RefreshFileActions();
        RefreshViewActions();
        RefreshEditActions();
    });

    window_.SetBeforeCloseHandler([this]() {
        return CloseCurrentPlaceIfAllowed();
    });

    placeManager_.PlaceOpened.Connect([this](const std::shared_ptr<Engine::DataModel::Place>&) {
        RefreshFileActions();
        RefreshViewActions();
        RefreshEditActions();
    });
    placeManager_.PlaceClosed.Connect([this](const std::shared_ptr<Engine::DataModel::Place>&) {
        RefreshFileActions();
        RefreshViewActions();
        RefreshEditActions();
    });
    RefreshFileActions();
}

void TopBarController::BuildEditMenu() {
    editMenu_ = new QMenu(editButton_);
    editButton_->setMenu(editMenu_);

    undoAction_ = editMenu_->addAction("Undo");
    Core::StudioShortcutManager::ApplyToAction(*undoAction_, Core::StudioShortcutAction::Undo);
    Core::StudioShortcutManager::ApplyIconToAction(*undoAction_, Core::StudioShortcutAction::Undo);
    QObject::connect(undoAction_, &QAction::triggered, &window_, [this]() {
        const auto place = placeManager_.GetCurrentPlace();
        if (place == nullptr) {
            return;
        }
        const auto service = std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(
            place->FindService("ChangeHistoryService")
        );
        if (service != nullptr) {
            service->Undo();
        }
    });

    redoAction_ = editMenu_->addAction("Redo");
    Core::StudioShortcutManager::ApplyToAction(*redoAction_, Core::StudioShortcutAction::Redo);
    Core::StudioShortcutManager::ApplyIconToAction(*redoAction_, Core::StudioShortcutAction::Redo);
    QObject::connect(redoAction_, &QAction::triggered, &window_, [this]() {
        const auto place = placeManager_.GetCurrentPlace();
        if (place == nullptr) {
            return;
        }
        const auto service = std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(
            place->FindService("ChangeHistoryService")
        );
        if (service != nullptr) {
            service->Redo();
        }
    });

    editMenu_->addSeparator();

    cutAction_ = editMenu_->addAction("Cut");
    Core::StudioShortcutManager::ApplyToAction(*cutAction_, Core::StudioShortcutAction::Cut);
    Core::StudioShortcutManager::ApplyIconToAction(*cutAction_, Core::StudioShortcutAction::Cut);
    QObject::connect(cutAction_, &QAction::triggered, &window_, [this]() {
        if (quickActions_ != nullptr) {
            quickActions_->EditCut();
        }
    });

    copyAction_ = editMenu_->addAction("Copy");
    Core::StudioShortcutManager::ApplyToAction(*copyAction_, Core::StudioShortcutAction::Copy);
    Core::StudioShortcutManager::ApplyIconToAction(*copyAction_, Core::StudioShortcutAction::Copy);
    QObject::connect(copyAction_, &QAction::triggered, &window_, [this]() {
        if (quickActions_ != nullptr) {
            quickActions_->EditCopy();
        }
    });

    pasteAction_ = editMenu_->addAction("Paste");
    Core::StudioShortcutManager::ApplyToAction(*pasteAction_, Core::StudioShortcutAction::Paste);
    Core::StudioShortcutManager::ApplyIconToAction(*pasteAction_, Core::StudioShortcutAction::Paste);
    QObject::connect(pasteAction_, &QAction::triggered, &window_, [this]() {
        if (quickActions_ != nullptr) {
            quickActions_->EditPaste();
        }
    });

    editMenu_->addSeparator();

    deleteAction_ = editMenu_->addAction("Delete");
    Core::StudioShortcutManager::ApplyToAction(*deleteAction_, Core::StudioShortcutAction::Delete);
    Core::StudioShortcutManager::ApplyIconToAction(*deleteAction_, Core::StudioShortcutAction::Delete);
    QObject::connect(deleteAction_, &QAction::triggered, &window_, [this]() {
        if (quickActions_ != nullptr) {
            quickActions_->EditDelete();
        }
    });

    selectAllAction_ = editMenu_->addAction("Select All");
    Core::StudioShortcutManager::ApplyToAction(*selectAllAction_, Core::StudioShortcutAction::SelectAll);
    Core::StudioShortcutManager::ApplyIconToAction(*selectAllAction_, Core::StudioShortcutAction::SelectAll);
    QObject::connect(selectAllAction_, &QAction::triggered, &window_, [this]() {
        if (quickActions_ != nullptr) {
            quickActions_->EditSelectAll();
        }
    });

    duplicateAction_ = editMenu_->addAction("Duplicate");
    Core::StudioShortcutManager::ApplyToAction(*duplicateAction_, Core::StudioShortcutAction::Duplicate);
    Core::StudioShortcutManager::ApplyIconToAction(*duplicateAction_, Core::StudioShortcutAction::Duplicate);
    QObject::connect(duplicateAction_, &QAction::triggered, &window_, [this]() {
        if (quickActions_ != nullptr) {
            quickActions_->EditDuplicate();
        }
    });

    QObject::connect(editMenu_, &QMenu::aboutToShow, &window_, [this]() { RefreshEditActions(); });
    RefreshEditActions();
}

void TopBarController::BuildToolsMenu() {
    toolsMenu_ = new QMenu(toolsButton_);
    toolsButton_->setMenu(toolsMenu_);

    settingsWidget_ = std::make_unique<Widgets::Settings::SettingsWidget>(&window_);
    aboutDialog_ = std::make_unique<Widgets::AboutStudioDialog>(&window_);

    auto* settingsAction = toolsMenu_->addAction(Core::GetIconPackManager().GetIcon("cog.png"), "Studio Settings");
    QObject::connect(settingsAction, &QAction::triggered, &window_, [this]() {
        settingsWidget_->show();
        settingsWidget_->raise();
        settingsWidget_->activateWindow();
    });

    auto* aboutAction = toolsMenu_->addAction(Core::GetIconPackManager().GetIcon("information.png"), "About Studio");
    QObject::connect(aboutAction, &QAction::triggered, &window_, [this]() {
        aboutDialog_->show();
        aboutDialog_->raise();
        aboutDialog_->activateWindow();
    });
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
    if (saveAsTomlAction_ != nullptr) {
        saveAsTomlAction_->setEnabled(hasPlace);
    }
    if (closeAction_ != nullptr) {
        closeAction_->setVisible(hasPlace);
        closeAction_->setEnabled(hasPlace);
    }
}

void TopBarController::RefreshEditActions() {
    const auto place = placeManager_.GetCurrentPlace();
    const bool hasPlace = place != nullptr;

    if (undoAction_ != nullptr) {
        undoAction_->setEnabled(hasPlace);
    }
    if (redoAction_ != nullptr) {
        redoAction_->setEnabled(hasPlace);
    }

    bool canCopy = false;
    bool canCut = false;
    bool canDelete = false;
    bool canDuplicate = false;

    if (place != nullptr) {
        const auto selection = std::dynamic_pointer_cast<Engine::DataModel::Selection>(place->FindService("Selection"));
        if (selection != nullptr) {
            for (const auto& inst : selection->Get()) {
                if (inst == nullptr || inst->IsService() || !inst->IsInsertable()) {
                    continue;
                }
                canCopy = true;
                if (inst->GetParent() != nullptr) {
                    canCut = true;
                    canDelete = true;
                    canDuplicate = true;
                }
            }
        }
    }

    const bool canQuickAction = hasPlace && quickActions_ != nullptr;
    if (cutAction_ != nullptr) {
        cutAction_->setEnabled(canQuickAction && canCut);
    }
    if (copyAction_ != nullptr) {
        copyAction_->setEnabled(canQuickAction && canCopy);
    }
    if (pasteAction_ != nullptr) {
        pasteAction_->setEnabled(canQuickAction);
    }
    if (deleteAction_ != nullptr) {
        deleteAction_->setEnabled(canQuickAction && canDelete);
    }
    if (selectAllAction_ != nullptr) {
        selectAllAction_->setEnabled(canQuickAction);
    }
    if (duplicateAction_ != nullptr) {
        duplicateAction_->setEnabled(canQuickAction && canDuplicate);
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

    QString defaultPath = Engine::Core::QtBridge::ToQString(currentPlace->GetFilePath());
    if (defaultPath.isEmpty()) {
        defaultPath = Engine::Core::QtBridge::ToQString(Engine::Core::PlaceFileUtils::DefaultUntitledFileName());
    }
    QString selectedPath = QFileDialog::getSaveFileName(
        &window_,
        "Save Place to File",
        defaultPath,
        Engine::Core::QtBridge::ToQString(Engine::Core::PlaceFileUtils::FileDialogSaveBinaryFilter())
    );
    if (selectedPath.isEmpty()) {
        return {};
    }

    return Engine::Core::QtBridge::ToQString(
        Engine::Core::PlaceFileUtils::EnsureExtension(
            Engine::Core::QtBridge::ToStdString(std::move(selectedPath)),
            Engine::Core::PlaceFileUtils::BinaryExtension()
        )
    );
}

QString TopBarController::PromptSaveTomlPath() const {
    const auto currentPlace = placeManager_.GetCurrentPlace();
    if (currentPlace == nullptr) {
        return {};
    }

    QString defaultPath = Engine::Core::QtBridge::ToQString(currentPlace->GetFilePath());
    if (defaultPath.isEmpty()) {
        defaultPath = Engine::Core::QtBridge::ToQString(Engine::Core::PlaceFileUtils::DefaultUntitledTomlFileName());
    }

    QString selectedPath = QFileDialog::getSaveFileName(
        &window_,
        "Save Place to File (TOML)",
        defaultPath,
        Engine::Core::QtBridge::ToQString(Engine::Core::PlaceFileUtils::FileDialogSaveTomlFilter())
    );
    if (selectedPath.isEmpty()) {
        return {};
    }

    return Engine::Core::QtBridge::ToQString(
        Engine::Core::PlaceFileUtils::EnsureExtension(
            Engine::Core::QtBridge::ToStdString(std::move(selectedPath)),
            "toml"
        )
    );
}

void TopBarController::SaveCurrentPlaceToPath(const QString& path) const {
    if (path.isEmpty()) {
        return;
    }

    window_.ShowBusy("Saving place...");
    try {
        placeManager_.SaveCurrentPlaceToFile(Engine::Core::QtBridge::ToStdString(path));
        window_.HideBusy("Ready");
    } catch (...) {
        window_.HideBusy();
        throw;
    }
}

void TopBarController::SaveCurrentPlaceToPathAs(const QString& path, const Engine::DataModel::Place::FileFormat format) const {
    if (path.isEmpty()) {
        return;
    }

    window_.ShowBusy("Saving place...");
    try {
        placeManager_.SaveCurrentPlaceToFileAs(Engine::Core::QtBridge::ToStdString(path), format);
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

bool TopBarController::SaveCurrentPlaceAsTomlWithDialog() const {
    const auto currentPlace = placeManager_.GetCurrentPlace();
    if (currentPlace == nullptr) {
        return false;
    }

    const QString outputPath = PromptSaveTomlPath();
    if (outputPath.isEmpty()) {
        return false;
    }

    SaveCurrentPlaceToPathAs(outputPath, Engine::DataModel::Place::FileFormat::Toml);
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
