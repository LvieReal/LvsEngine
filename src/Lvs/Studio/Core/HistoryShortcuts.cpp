#include "Lvs/Studio/Core/HistoryShortcuts.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Studio/Core/StudioShortcutManager.hpp"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QWidget>

#include <memory>

namespace Lvs::Studio::Core {

HistoryShortcuts::HistoryShortcuts(QApplication& app, QWidget& window, const Engine::EngineContextPtr& context)
    : QObject(&app),
      context_(context) {
    undoAction_ = new QAction(&window);
    StudioShortcutManager::ApplyToAction(*undoAction_, StudioShortcutAction::Undo);
    undoAction_->setShortcutContext(Qt::ApplicationShortcut);
    QObject::connect(undoAction_, &QAction::triggered, &window, [this]() { Undo(); });
    window.addAction(undoAction_);

    redoAction_ = new QAction(&window);
    StudioShortcutManager::ApplyToAction(*redoAction_, StudioShortcutAction::Redo);
    redoAction_->setShortcutContext(Qt::ApplicationShortcut);
    QObject::connect(redoAction_, &QAction::triggered, &window, [this]() { Redo(); });
    window.addAction(redoAction_);

    app.installEventFilter(this);
}

HistoryShortcuts::~HistoryShortcuts() = default;

void HistoryShortcuts::SetContext(const Engine::EngineContextPtr& context) {
    context_ = context;
}

bool HistoryShortcuts::eventFilter(QObject* watched, QEvent* event) {
    static_cast<void>(watched);

    if (event == nullptr) {
        return false;
    }
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride) {
        return false;
    }

    auto* keyEvent = dynamic_cast<QKeyEvent*>(event);
    if (keyEvent == nullptr) {
        return false;
    }

    const bool isUndo = StudioShortcutManager::Matches(StudioShortcutAction::Undo, *keyEvent);
    const bool isRedo = StudioShortcutManager::Matches(StudioShortcutAction::Redo, *keyEvent);
    if (!isUndo && !isRedo) {
        return false;
    }

    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }

    if (isUndo) {
        Undo();
    } else {
        Redo();
    }
    event->accept();
    return true;
}

void HistoryShortcuts::Undo() const {
    if (context_ == nullptr || context_->PlaceManager == nullptr) {
        return;
    }

    const auto place = context_->PlaceManager->GetCurrentPlace();
    if (place == nullptr) {
        return;
    }

    const auto service = std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(
        place->FindService("ChangeHistoryService")
    );
    if (service != nullptr) {
        service->Undo();
    }
}

void HistoryShortcuts::Redo() const {
    if (context_ == nullptr || context_->PlaceManager == nullptr) {
        return;
    }

    const auto place = context_->PlaceManager->GetCurrentPlace();
    if (place == nullptr) {
        return;
    }

    const auto service = std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(
        place->FindService("ChangeHistoryService")
    );
    if (service != nullptr) {
        service->Redo();
    }
}

} // namespace Lvs::Studio::Core
