#include "Lvs/Studio/Core/StudioQuickActions.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/RegularError.hpp"
#include "Lvs/Engine/Core/Viewport.hpp"
#include "Lvs/Engine/DataModel/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Engine/DataModel/Selection.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Studio/Controllers/ToolbarController.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"
#include "Lvs/Studio/Core/ViewportManager.hpp"
#include "Lvs/Studio/Widgets/Explorer/ExplorerWidget.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QTextEdit>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <exception>
#include <memory>

namespace Lvs::Studio::Core {

StudioQuickActions::StudioQuickActions(
    QApplication& app,
    QWidget& window,
    const Engine::EngineContextPtr& context
)
    : QObject(&app),
      context_(context),
      window_(&window) {
    app.installEventFilter(this);
}

StudioQuickActions::~StudioQuickActions() = default;

void StudioQuickActions::SetContext(const Engine::EngineContextPtr& context) {
    context_ = context;
}

bool StudioQuickActions::TryShowViewportContextMenu(Engine::Core::Viewport& viewport, const QPoint& globalPos) const {
    if (!viewport.hasFocus() && !viewport.underMouse()) {
        return false;
    }
    return ShowSelectedBasePartContextMenu(viewport, globalPos);
}

bool StudioQuickActions::TryShowExplorerContextMenu(
    Widgets::Explorer::ExplorerWidget& explorer,
    const QPoint& globalPos
) const {
    if (!explorer.hasFocus() && !explorer.underMouse()) {
        return false;
    }

    const auto place = GetCurrentPlace();
    const auto selectionService = GetSelectionService(place);
    if (selectionService == nullptr) {
        return false;
    }

    if (QTreeWidgetItem* item = explorer.itemAt(explorer.mapFromGlobal(globalPos)); item != nullptr) {
        const QString instanceId = item->data(0, Qt::UserRole).toString();
        if (!instanceId.isEmpty() && place != nullptr) {
            selectionService->Set(place->FindInstanceById(instanceId));
        }
    }

    return ShowSelectedBasePartContextMenu(explorer, globalPos);
}

bool StudioQuickActions::eventFilter(QObject* watched, QEvent* event) {
    if (event == nullptr) {
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (mouseEvent == nullptr || mouseEvent->button() != Qt::RightButton) {
            return false;
        }
        auto* explorer = ResolveExplorerWidgetFromObject(watched);
        if (explorer == nullptr) {
            return false;
        }
        if (TryShowExplorerContextMenu(*explorer, mouseEvent->globalPosition().toPoint())) {
            event->accept();
            return true;
        }
        return false;
    }

    if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride) {
        return false;
    }

    auto* keyEvent = dynamic_cast<QKeyEvent*>(event);
    if (keyEvent == nullptr || keyEvent->isAutoRepeat()) {
        return false;
    }
    if (IsTextInputFocused()) {
        return false;
    }

    const bool toolShortcut =
        IsToolShortcut(*keyEvent, Qt::Key_1) ||
        IsToolShortcut(*keyEvent, Qt::Key_2) ||
        IsToolShortcut(*keyEvent, Qt::Key_3);
    const bool duplicateShortcut = IsDuplicateShortcut(*keyEvent);
    const bool deleteShortcut = IsDeleteShortcut(*keyEvent);

    const bool quickActionShortcut = (duplicateShortcut || deleteShortcut) && IsQuickActionContext();
    if (!toolShortcut && !quickActionShortcut) {
        return false;
    }

    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }

    if (toolShortcut) {
        ActivateToolShortcut(keyEvent->key());
    } else if (duplicateShortcut) {
        DuplicateSelection();
    } else if (deleteShortcut) {
        DeleteSelection();
    } else {
        return false;
    }

    event->accept();
    return true;
}

std::shared_ptr<Engine::DataModel::Place> StudioQuickActions::GetCurrentPlace() const {
    if (context_ == nullptr || context_->PlaceManager == nullptr) {
        return nullptr;
    }
    return context_->PlaceManager->GetCurrentPlace();
}

std::shared_ptr<Engine::DataModel::Selection> StudioQuickActions::GetSelectionService(
    const std::shared_ptr<Engine::DataModel::Place>& place
) const {
    if (place == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Engine::DataModel::Selection>(place->FindService("Selection"));
}

std::shared_ptr<Engine::DataModel::ChangeHistoryService> StudioQuickActions::GetHistoryService(
    const std::shared_ptr<Engine::DataModel::Place>& place
) const {
    if (place == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(place->FindService("ChangeHistoryService"));
}

std::shared_ptr<Engine::Objects::BasePart> StudioQuickActions::GetSelectedBasePart() const {
    const auto place = GetCurrentPlace();
    const auto selection = GetSelectionService(place);
    if (selection == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Engine::Objects::BasePart>(selection->GetPrimary());
}

bool StudioQuickActions::IsTextInputFocused() const {
    const auto* focused = QApplication::focusWidget();
    if (focused == nullptr) {
        return false;
    }
    if (qobject_cast<const QLineEdit*>(focused) != nullptr) {
        return true;
    }
    if (qobject_cast<const QTextEdit*>(focused) != nullptr) {
        return true;
    }
    if (qobject_cast<const QPlainTextEdit*>(focused) != nullptr) {
        return true;
    }
    if (qobject_cast<const QAbstractSpinBox*>(focused) != nullptr) {
        return true;
    }
    if (const auto* combo = qobject_cast<const QComboBox*>(focused); combo != nullptr && combo->isEditable()) {
        return true;
    }
    return false;
}

bool StudioQuickActions::IsViewportShortcutContext() const {
    if (context_ == nullptr || context_->ViewportManager == nullptr) {
        return false;
    }
    Engine::Core::Viewport* viewport = context_->ViewportManager->GetViewport();
    if (viewport == nullptr || !viewport->isVisible()) {
        return false;
    }
    return viewport->hasFocus() || viewport->underMouse();
}

bool StudioQuickActions::IsExplorerShortcutContext() const {
    QWidget* focused = QApplication::focusWidget();
    if (ResolveExplorerWidgetFromObject(focused) != nullptr) {
        return true;
    }
    return ResolveExplorerWidgetFromObject(QApplication::widgetAt(QCursor::pos())) != nullptr;
}

bool StudioQuickActions::IsQuickActionContext() const {
    return IsViewportShortcutContext() || IsExplorerShortcutContext();
}

bool StudioQuickActions::IsToolShortcut(const QKeyEvent& event, const int key) const {
    const Qt::KeyboardModifiers mods = event.modifiers();
    const bool hasOnlyShift = mods.testFlag(Qt::ShiftModifier) &&
        !mods.testFlag(Qt::ControlModifier) &&
        !mods.testFlag(Qt::AltModifier) &&
        !mods.testFlag(Qt::MetaModifier);
    const bool hasNoMods = mods == Qt::NoModifier;
    return event.key() == key && (hasNoMods || hasOnlyShift);
}

bool StudioQuickActions::IsDuplicateShortcut(const QKeyEvent& event) const {
    const Qt::KeyboardModifiers mods = event.modifiers();
    return mods.testFlag(Qt::ControlModifier) &&
        !mods.testFlag(Qt::ShiftModifier) &&
        !mods.testFlag(Qt::AltModifier) &&
        !mods.testFlag(Qt::MetaModifier) &&
        event.key() == Qt::Key_D;
}

bool StudioQuickActions::IsDeleteShortcut(const QKeyEvent& event) const {
    const Qt::KeyboardModifiers mods = event.modifiers();
    return mods == Qt::NoModifier && event.key() == Qt::Key_Delete;
}

void StudioQuickActions::ActivateToolShortcut(const int key) const {
    if (context_ == nullptr) {
        return;
    }

    Engine::Core::Tool tool = Engine::Core::Tool::SelectTool;
    if (key == Qt::Key_1) {
        tool = Engine::Core::Tool::SelectTool;
    } else if (key == Qt::Key_2) {
        tool = Engine::Core::Tool::MoveTool;
    } else if (key == Qt::Key_3) {
        tool = Engine::Core::Tool::SizeTool;
    } else {
        return;
    }

    if (context_->ToolbarController != nullptr) {
        context_->ToolbarController->ActivateTool(tool);
        return;
    }
    if (context_->EditorToolState != nullptr) {
        context_->EditorToolState->SetTool(tool);
    }
}

void StudioQuickActions::DeleteSelection() const {
    try {
        const auto selected = GetSelectedBasePart();
        if (selected == nullptr || selected->GetParent() == nullptr) {
            return;
        }

        const auto place = GetCurrentPlace();
        const auto historyService = GetHistoryService(place);
        const auto selectionService = GetSelectionService(place);

        auto command = std::make_shared<Engine::Utils::ReparentCommand>(selected, nullptr);
        if (historyService == nullptr) {
            command->Do();
        } else if (historyService->IsRecording()) {
            historyService->Record(command);
        } else {
            historyService->BeginRecording("Delete");
            try {
                historyService->Record(command);
                historyService->FinishRecording();
            } catch (...) {
                historyService->FinishRecording();
                throw;
            }
        }

        if (selectionService != nullptr) {
            selectionService->Clear();
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::DuplicateSelection() const {
    try {
        const auto selected = GetSelectedBasePart();
        if (selected == nullptr) {
            return;
        }
        const auto parent = selected->GetParent();
        if (parent == nullptr) {
            return;
        }

        const auto clone = CloneRecursive(selected);
        if (clone == nullptr || !clone->CanParentTo(parent) || !parent->CanAcceptChild(clone)) {
            return;
        }

        const auto place = GetCurrentPlace();
        const auto historyService = GetHistoryService(place);
        const auto selectionService = GetSelectionService(place);

        auto command = std::make_shared<Engine::Utils::ReparentCommand>(clone, parent);
        if (historyService == nullptr) {
            command->Do();
        } else if (historyService->IsRecording()) {
            historyService->Record(command);
        } else {
            historyService->BeginRecording("Duplicate");
            try {
                historyService->Record(command);
                historyService->FinishRecording();
            } catch (...) {
                historyService->FinishRecording();
                throw;
            }
        }

        if (selectionService != nullptr) {
            selectionService->Set(clone);
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::PopulateInsertMenu(
    QMenu& menu,
    const std::shared_ptr<Engine::Core::Instance>& parent
) const {
    if (parent == nullptr) {
        return;
    }

    bool hasAction = false;
    const auto classGroups = Engine::DataModel::ClassRegistry::GetClassesByCategory();
    for (auto categoryIt = classGroups.cbegin(); categoryIt != classGroups.cend(); ++categoryIt) {
        auto* categoryMenu = menu.addMenu(categoryIt.key());
        bool hasCategoryAction = false;

        for (const auto& classInfo : categoryIt.value()) {
            const auto probe = classInfo.Factory();
            if (probe == nullptr || !probe->IsInsertable()) {
                continue;
            }
            if (!probe->CanParentTo(parent) || !parent->CanAcceptChild(probe)) {
                continue;
            }

            QAction* action = categoryMenu->addAction(classInfo.Name);
            const QPixmap icon = GetIconPackManager().GetPixmapForInstance(probe);
            if (!icon.isNull()) {
                action->setIcon(QIcon(icon));
            }

            QObject::connect(action, &QAction::triggered, window_, [this, parent, className = classInfo.Name]() {
                InsertObject(parent, className);
            });

            hasAction = true;
            hasCategoryAction = true;
        }

        if (!hasCategoryAction) {
            delete categoryMenu;
        }
    }

    if (!hasAction) {
        menu.clear();
    }
}

void StudioQuickActions::InsertObject(
    const std::shared_ptr<Engine::Core::Instance>& parent,
    const QString& className
) const {
    try {
        if (parent == nullptr || className.isEmpty()) {
            return;
        }

        const auto created = Engine::DataModel::ClassRegistry::CreateInstance(className);
        if (created == nullptr || !created->IsInsertable()) {
            return;
        }
        if (!created->CanParentTo(parent) || !parent->CanAcceptChild(created)) {
            return;
        }

        const auto place = GetCurrentPlace();
        const auto historyService = GetHistoryService(place);
        const auto selectionService = GetSelectionService(place);

        auto command = std::make_shared<Engine::Utils::ReparentCommand>(created, parent);
        if (historyService == nullptr) {
            command->Do();
        } else if (historyService->IsRecording()) {
            historyService->Record(command);
        } else {
            historyService->BeginRecording("Insert");
            try {
                historyService->Record(command);
                historyService->FinishRecording();
            } catch (...) {
                historyService->FinishRecording();
                throw;
            }
        }

        if (selectionService != nullptr) {
            selectionService->Set(created);
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

bool StudioQuickActions::ShowSelectedBasePartContextMenu(QWidget& owner, const QPoint& globalPos) const {
    const auto selected = GetSelectedBasePart();
    if (selected == nullptr || selected->GetParent() == nullptr) {
        return false;
    }

    QMenu menu(&owner);

    auto* insertMenu = menu.addMenu("Insert Object");
    PopulateInsertMenu(*insertMenu, selected);
    if (insertMenu->isEmpty()) {
        menu.removeAction(insertMenu->menuAction());
        delete insertMenu;
    }

    menu.addSeparator();
    QAction* duplicateAction = menu.addAction("Duplicate");
    duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    QObject::connect(duplicateAction, &QAction::triggered, &owner, [this]() { DuplicateSelection(); });

    QAction* deleteAction = menu.addAction("Delete");
    QObject::connect(deleteAction, &QAction::triggered, &owner, [this]() { DeleteSelection(); });

    if (menu.actions().isEmpty()) {
        return false;
    }

    menu.exec(globalPos);
    return true;
}

Widgets::Explorer::ExplorerWidget* StudioQuickActions::ResolveExplorerWidgetFromObject(QObject* object) const {
    QObject* current = object;
    while (current != nullptr) {
        if (auto* explorer = dynamic_cast<Widgets::Explorer::ExplorerWidget*>(current); explorer != nullptr) {
            return explorer;
        }
        current = current->parent();
    }
    return nullptr;
}

std::shared_ptr<Engine::Core::Instance> StudioQuickActions::CloneRecursive(
    const std::shared_ptr<Engine::Core::Instance>& source
) const {
    if (source == nullptr) {
        return nullptr;
    }

    const auto clone = Engine::DataModel::ClassRegistry::CreateInstance(source->GetClassDescriptor().ClassName());
    if (clone == nullptr) {
        return nullptr;
    }

    const auto& properties = source->GetProperties();
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        const auto& definition = it->Definition();
        if (definition.ReadOnly || definition.Name == "ClassName" || definition.IsInstanceReference) {
            continue;
        }
        clone->SetProperty(definition.Name, it->Get());
    }

    for (const auto& child : source->GetChildren()) {
        if (child == nullptr || !child->IsInsertable()) {
            continue;
        }
        const auto childClone = CloneRecursive(child);
        if (childClone == nullptr || !childClone->CanParentTo(clone) || !clone->CanAcceptChild(childClone)) {
            continue;
        }
        childClone->SetParent(clone);
    }

    return clone;
}

} // namespace Lvs::Studio::Core
