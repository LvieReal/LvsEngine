#include "Lvs/Studio/Core/StudioQuickActions.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/RegularError.hpp"
#include "Lvs/Engine/Core/Viewport.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Engine/DataModel/Services/Selection.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Engine/Utils/InstanceSelection.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"
#include "Lvs/Studio/Controllers/ToolbarController.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"
#include "Lvs/Studio/Core/StudioShortcutManager.hpp"
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
#include <unordered_set>
#include <array>
#include <cmath>
#include <vector>

namespace Lvs::Studio::Core {

namespace {

struct HitFace {
    int Axis{0}; // 0=x,1=y,2=z
    double Sign{1.0}; // +1 max face, -1 min face
};

HitFace FindClosestHitFace(const Engine::Math::AABB& aabb, const Engine::Math::Vector3& point) {
    const std::array<double, 6> distances{
        std::abs(point.x - aabb.Min.x),
        std::abs(point.x - aabb.Max.x),
        std::abs(point.y - aabb.Min.y),
        std::abs(point.y - aabb.Max.y),
        std::abs(point.z - aabb.Min.z),
        std::abs(point.z - aabb.Max.z)
    };

    std::size_t best = 0;
    double bestDistance = distances[0];
    for (std::size_t i = 1; i < distances.size(); ++i) {
        if (distances[i] < bestDistance) {
            bestDistance = distances[i];
            best = i;
        }
    }

    switch (best) {
        case 0: return HitFace{.Axis = 0, .Sign = -1.0};
        case 1: return HitFace{.Axis = 0, .Sign = 1.0};
        case 2: return HitFace{.Axis = 1, .Sign = -1.0};
        case 3: return HitFace{.Axis = 1, .Sign = 1.0};
        case 4: return HitFace{.Axis = 2, .Sign = -1.0};
        case 5: return HitFace{.Axis = 2, .Sign = 1.0};
        default: return HitFace{};
    }
}

Engine::Math::Vector3 AxisNormal(const HitFace& face) {
    if (face.Axis == 0) {
        return {face.Sign, 0.0, 0.0};
    }
    if (face.Axis == 1) {
        return {0.0, face.Sign, 0.0};
    }
    return {0.0, 0.0, face.Sign};
}

} // namespace

StudioQuickActions::StudioQuickActions(
    QApplication& app,
    QWidget& window,
    const Engine::EngineContextPtr& context,
    Engine::Core::Viewport* viewport,
    Controllers::ToolbarController* toolbarController
)
    : QObject(nullptr),
      context_(context),
      app_(&app),
      viewport_(viewport),
      toolbarController_(toolbarController),
      window_(&window) {
    app.installEventFilter(this);
}

StudioQuickActions::~StudioQuickActions() {
    // QObject destruction automatically detaches installed event filters.
    // Avoid touching QApplication during process teardown, where destruction
    // order may already have invalidated the application object.
}

void StudioQuickActions::SetContext(const Engine::EngineContextPtr& context) {
    context_ = context;
}

bool StudioQuickActions::TryShowViewportContextMenu(Engine::Core::Viewport& viewport, const QPoint& globalPos) const {
    if (!viewport.hasFocus() && !viewport.underMouse()) {
        return false;
    }

    const auto selected = GetSelectedInstance();
    const auto place = GetCurrentPlace();
    const auto insertParent = selected != nullptr
        ? selected
        : (place != nullptr ? place->FindService("Workspace") : nullptr);

    return ShowContextMenu(viewport, globalPos, selected, insertParent);
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

    const auto selected = GetSelectedInstance();
    const auto insertParent = selected != nullptr ? selected : (place != nullptr ? place->FindService("Workspace") : nullptr);
    return ShowContextMenu(explorer, globalPos, selected, insertParent);
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

        if (auto* explorer = ResolveExplorerWidgetFromObject(watched); explorer != nullptr) {
            if (TryShowExplorerContextMenu(*explorer, mouseEvent->globalPosition().toPoint())) {
                event->accept();
                return true;
            }
            return false;
        }

        if (auto* viewport = ResolveViewportFromObject(watched); viewport != nullptr) {
            if (viewport->WasRightMousePanned()) {
                return false;
            }
            if (TryShowViewportContextMenu(*viewport, mouseEvent->globalPosition().toPoint())) {
                return false;
            }
            return false;
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

    const bool quickActionContext = IsQuickActionContext();

    const bool toolShortcut =
        StudioShortcutManager::Matches(StudioShortcutAction::ToolSelect, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::ToolMove, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::ToolSize, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::ToggleLocalSpace, *keyEvent);

    const bool quickActionShortcut = quickActionContext && (
        StudioShortcutManager::Matches(StudioShortcutAction::Duplicate, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::Delete, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::Group, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::Ungroup, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::Copy, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::Cut, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::PasteInto, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::Paste, *keyEvent) ||
        StudioShortcutManager::Matches(StudioShortcutAction::FocusOnSelection, *keyEvent)
    );

    if (!toolShortcut && !quickActionShortcut) {
        return false;
    }

    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }

    if (toolShortcut) {
        if (StudioShortcutManager::Matches(StudioShortcutAction::ToolSelect, *keyEvent)) {
            ActivateTool(Engine::Core::Tool::SelectTool);
        } else if (StudioShortcutManager::Matches(StudioShortcutAction::ToolMove, *keyEvent)) {
            ActivateTool(Engine::Core::Tool::MoveTool);
        } else if (StudioShortcutManager::Matches(StudioShortcutAction::ToolSize, *keyEvent)) {
            ActivateTool(Engine::Core::Tool::SizeTool);
        } else if (StudioShortcutManager::Matches(StudioShortcutAction::ToggleLocalSpace, *keyEvent)) {
            if (context_ != nullptr && context_->EditorToolState != nullptr) {
                context_->EditorToolState->ToggleLocalSpace();
            }
        }
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Duplicate, *keyEvent)) {
        DuplicateSelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Delete, *keyEvent)) {
        DeleteSelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Group, *keyEvent)) {
        GroupSelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Ungroup, *keyEvent)) {
        UngroupSelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Copy, *keyEvent)) {
        CopySelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Cut, *keyEvent)) {
        CutSelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::PasteInto, *keyEvent)) {
        PasteSelectionIntoSelection();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::Paste, *keyEvent)) {
        PasteSelectionToTopmostService();
    } else if (StudioShortcutManager::Matches(StudioShortcutAction::FocusOnSelection, *keyEvent)) {
        FocusOnSelection();
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

std::shared_ptr<Engine::Core::Instance> StudioQuickActions::GetSelectedInstance() const {
    const auto place = GetCurrentPlace();
    const auto selection = GetSelectionService(place);
    if (selection == nullptr) {
        return nullptr;
    }
    return selection->GetPrimary();
}

static std::vector<std::shared_ptr<Engine::Core::Instance>> GetSelectedInstances(
    const std::shared_ptr<Engine::DataModel::Selection>& selection
) {
    if (selection == nullptr) {
        return {};
    }
    return selection->Get();
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
    return std::dynamic_pointer_cast<Engine::Objects::BasePart>(GetSelectedInstance());
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
    if (viewport_ == nullptr || !viewport_->isVisible()) {
        return false;
    }
    return viewport_->hasFocus() || viewport_->underMouse();
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

void StudioQuickActions::ActivateTool(const Engine::Core::Tool tool) const {
    if (context_ == nullptr) {
        return;
    }

    if (toolbarController_ != nullptr) {
        toolbarController_->ActivateTool(tool);
        return;
    }
    if (context_->EditorToolState != nullptr) {
        context_->EditorToolState->SetTool(tool);
    }
}

void StudioQuickActions::DeleteSelection() const {
    try {
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        const auto historyService = GetHistoryService(place);

        const auto selectedInstances = GetSelectedInstances(selectionService);
        std::vector<std::shared_ptr<Engine::Core::Instance>> targets;
        targets.reserve(selectedInstances.size());
        for (const auto& inst : selectedInstances) {
            if (inst == nullptr || !inst->IsInsertable() || inst->GetParent() == nullptr) {
                continue;
            }
            targets.push_back(inst);
        }
        if (targets.empty()) {
            return;
        }

        auto recordAll = [&](const bool beginGroup) {
            if (historyService != nullptr && beginGroup) {
                historyService->BeginRecording("Delete");
            }
            try {
                for (const auto& target : targets) {
                    auto command = std::make_shared<Engine::Utils::ReparentCommand>(target, nullptr);
                    if (historyService == nullptr) {
                        command->Do();
                    } else {
                        historyService->Record(command);
                    }
                }
            } catch (...) {
                if (historyService != nullptr && beginGroup) {
                    historyService->FinishRecording();
                }
                throw;
            }
            if (historyService != nullptr && beginGroup) {
                historyService->FinishRecording();
            }
        };

        if (historyService == nullptr) {
            recordAll(false);
        } else if (historyService->IsRecording()) {
            recordAll(false);
        } else {
            recordAll(true);
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
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        const auto historyService = GetHistoryService(place);

        const auto selectedInstances = GetSelectedInstances(selectionService);
        std::vector<std::pair<std::shared_ptr<Engine::Core::Instance>, std::shared_ptr<Engine::Core::Instance>>> toDuplicate;
        toDuplicate.reserve(selectedInstances.size());
        for (const auto& inst : selectedInstances) {
            if (inst == nullptr || !inst->IsInsertable()) {
                continue;
            }
            const auto parent = inst->GetParent();
            if (parent == nullptr) {
                continue;
            }
            toDuplicate.push_back({inst, parent});
        }
        if (toDuplicate.empty()) {
            return;
        }

        std::vector<std::shared_ptr<Engine::Core::Instance>> clones;
        clones.reserve(toDuplicate.size());

        auto recordAll = [&](const bool beginGroup) {
            if (historyService != nullptr && beginGroup) {
                historyService->BeginRecording("Duplicate");
            }
            try {
                for (const auto& [src, parent] : toDuplicate) {
                    const auto clone = CloneRecursive(src);
                    if (clone == nullptr || !clone->CanParentTo(parent) || !parent->CanAcceptChild(clone)) {
                        continue;
                    }
                    clones.push_back(clone);
                    auto command = std::make_shared<Engine::Utils::ReparentCommand>(clone, parent);
                    if (historyService == nullptr) {
                        command->Do();
                    } else {
                        historyService->Record(command);
                    }
                }
            } catch (...) {
                if (historyService != nullptr && beginGroup) {
                    historyService->FinishRecording();
                }
                throw;
            }
            if (historyService != nullptr && beginGroup) {
                historyService->FinishRecording();
            }
        };

        if (historyService == nullptr) {
            recordAll(false);
        } else if (historyService->IsRecording()) {
            recordAll(false);
        } else {
            recordAll(true);
        }

        if (selectionService != nullptr && !clones.empty()) {
            selectionService->Set(clones);
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::CopySelection() const {
    try {
        clipboardPrototypes_.clear();

        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        const auto selectedInstances = GetSelectedInstances(selectionService);
        for (const auto& inst : selectedInstances) {
            if (inst == nullptr || !inst->IsInsertable()) {
                continue;
            }
            const auto clone = CloneRecursive(inst);
            if (clone != nullptr) {
                clipboardPrototypes_.push_back(clone);
            }
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::CutSelection() const {
    CopySelection();
    DeleteSelection();
}

void StudioQuickActions::PasteSelectionToTopmostService() const {
    const auto contextInstance = GetSelectedInstance();
    PasteToParent(ResolveTopmostServicePasteParent(contextInstance), "Paste");
}

void StudioQuickActions::PasteSelectionIntoSelection() const {
    const auto place = GetCurrentPlace();
    const auto selectionService = GetSelectionService(place);
    const auto selectedInstances = GetSelectedInstances(selectionService);
    if (!selectedInstances.empty()) {
        PasteToParents(selectedInstances, "Paste Into");
        return;
    }
    PasteSelectionToTopmostService();
}

void StudioQuickActions::GroupSelection() const {
    try {
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        if (selectionService == nullptr) {
            return;
        }

        const auto historyService = GetHistoryService(place);

        std::vector<std::shared_ptr<Engine::Core::Instance>> groupable;
        groupable.reserve(selectionService->Get().size());
        for (const auto& inst : selectionService->Get()) {
            if (inst == nullptr || inst->IsService() || !inst->IsInsertable() || inst->GetParent() == nullptr) {
                continue;
            }
            groupable.push_back(inst);
        }

        groupable = Engine::Utils::FilterTopLevelInstances(groupable);
        if (groupable.empty()) {
            return;
        }

        auto targetParent = groupable.front()->GetParent();
        bool allSameParent = true;
        for (const auto& inst : groupable) {
            if (inst == nullptr) {
                continue;
            }
            if (inst->GetParent() != targetParent) {
                allSameParent = false;
                break;
            }
        }
        if (!allSameParent) {
            if (const auto primary = selectionService->GetPrimary(); primary != nullptr && primary->GetParent() != nullptr) {
                targetParent = primary->GetParent();
            }
        }
        if (targetParent == nullptr) {
            return;
        }

        bool hasAnyParts = false;
        for (const auto& inst : groupable) {
            if (inst == nullptr) {
                continue;
            }
            if (!Engine::Utils::CollectDescendantBaseParts(inst).empty()) {
                hasAnyParts = true;
                break;
            }
        }

        const QString className = hasAnyParts ? "Model" : "Folder";
        const auto group = Engine::DataModel::ClassRegistry::CreateInstance(className);
        if (group == nullptr) {
            return;
        }
        if (!group->CanParentTo(targetParent) || !targetParent->CanAcceptChild(group)) {
            return;
        }

        auto recordAll = [&](const bool beginGroup) {
            if (historyService != nullptr && beginGroup) {
                historyService->BeginRecording("Group");
            }
            try {
                auto parentCommand = std::make_shared<Engine::Utils::ReparentCommand>(group, targetParent);
                if (historyService == nullptr) {
                    parentCommand->Do();
                } else {
                    historyService->Record(parentCommand);
                }

                for (const auto& inst : groupable) {
                    if (inst == nullptr || inst->GetParent() == nullptr) {
                        continue;
                    }
                    if (!inst->CanParentTo(group) || !group->CanAcceptChild(inst)) {
                        continue;
                    }
                    auto command = std::make_shared<Engine::Utils::ReparentCommand>(inst, group);
                    if (historyService == nullptr) {
                        command->Do();
                    } else {
                        historyService->Record(command);
                    }
                }
            } catch (...) {
                if (historyService != nullptr && beginGroup) {
                    historyService->FinishRecording();
                }
                throw;
            }
            if (historyService != nullptr && beginGroup) {
                historyService->FinishRecording();
            }
        };

        if (historyService == nullptr) {
            recordAll(false);
        } else if (historyService->IsRecording()) {
            recordAll(false);
        } else {
            recordAll(true);
        }

        selectionService->Set(group);
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::UngroupSelection() const {
    try {
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        if (selectionService == nullptr) {
            return;
        }

        const auto historyService = GetHistoryService(place);

        std::vector<std::shared_ptr<Engine::Core::Instance>> groups;
        groups.reserve(selectionService->Get().size());
        for (const auto& inst : selectionService->Get()) {
            if (inst == nullptr || inst->GetParent() == nullptr) {
                continue;
            }
            const QString cls = inst->GetClassName();
            if (cls != "Model" && cls != "Folder") {
                continue;
            }
            groups.push_back(inst);
        }

        groups = Engine::Utils::FilterTopLevelInstances(groups);
        if (groups.empty()) {
            return;
        }

        std::vector<std::shared_ptr<Engine::Core::Instance>> moved;

        auto recordAll = [&](const bool beginGroup) {
            if (historyService != nullptr && beginGroup) {
                historyService->BeginRecording("Ungroup");
            }
            try {
                for (const auto& group : groups) {
                    if (group == nullptr || group->GetParent() == nullptr) {
                        continue;
                    }
                    const auto parent = group->GetParent();
                    if (parent == nullptr) {
                        continue;
                    }

                    const auto children = group->GetChildren();
                    for (const auto& child : children) {
                        if (child == nullptr) {
                            continue;
                        }
                        if (!child->CanParentTo(parent) || !parent->CanAcceptChild(child)) {
                            continue;
                        }
                        moved.push_back(child);
                        auto command = std::make_shared<Engine::Utils::ReparentCommand>(child, parent);
                        if (historyService == nullptr) {
                            command->Do();
                        } else {
                            historyService->Record(command);
                        }
                    }

                    auto removeGroupCommand = std::make_shared<Engine::Utils::ReparentCommand>(group, nullptr);
                    if (historyService == nullptr) {
                        removeGroupCommand->Do();
                    } else {
                        historyService->Record(removeGroupCommand);
                    }
                }
            } catch (...) {
                if (historyService != nullptr && beginGroup) {
                    historyService->FinishRecording();
                }
                throw;
            }
            if (historyService != nullptr && beginGroup) {
                historyService->FinishRecording();
            }
        };

        if (historyService == nullptr) {
            recordAll(false);
        } else if (historyService->IsRecording()) {
            recordAll(false);
        } else {
            recordAll(true);
        }

        moved = Engine::Utils::FilterTopLevelInstances(moved);
        if (!moved.empty()) {
            selectionService->Set(moved);
        } else {
            selectionService->Clear();
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::FocusOnSelection() const {
    if (viewport_ == nullptr) {
        return;
    }

    const auto place = GetCurrentPlace();
    const auto selectionService = GetSelectionService(place);
    if (selectionService == nullptr) {
        return;
    }

    const auto topLevelSelected = Engine::Utils::FilterTopLevelInstances(selectionService->Get());
    const auto parts = Engine::Utils::CollectBasePartsFromInstances(topLevelSelected);
    const auto bounds = Engine::Utils::ComputeCombinedWorldAABB(parts);
    if (!bounds.has_value()) {
        return;
    }

    viewport_->FocusOnBounds(bounds.value());
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
                InsertObject(parent, className, QCursor::pos());
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
    const QString& className,
    const QPoint& spawnGlobalPos
) const {
    try {
        static_cast<void>(spawnGlobalPos);
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

        if (const auto createdPart = std::dynamic_pointer_cast<Engine::Objects::BasePart>(created); createdPart != nullptr &&
            viewport_ != nullptr && place != nullptr) {
            const auto workspace = std::dynamic_pointer_cast<Engine::DataModel::Workspace>(place->FindService("Workspace"));
            const auto camera = workspace != nullptr
                ? workspace->GetProperty("CurrentCamera").value<std::shared_ptr<Engine::Objects::Camera>>()
                : nullptr;

            if (camera != nullptr && viewport_->isVisible() && viewport_->width() > 0 && viewport_->height() > 0) {
                const QPoint local = QPoint(viewport_->width() / 2, viewport_->height() / 2);

                const Engine::Utils::Ray ray = Engine::Utils::ScreenPointToRay(
                    static_cast<double>(local.x()),
                    static_cast<double>(local.y()),
                    viewport_->width(),
                    viewport_->height(),
                    camera
                );

                std::vector<std::shared_ptr<Engine::Objects::BasePart>> parts;
                if (workspace != nullptr) {
                    for (const auto& descendant : workspace->GetDescendants()) {
                        const auto part = std::dynamic_pointer_cast<Engine::Objects::BasePart>(descendant);
                        if (part == nullptr || part->GetParent() == nullptr) {
                            continue;
                        }
                        parts.push_back(part);
                    }
                }

                const auto [hitPart, hitDistance] = Engine::Utils::RaycastParts(ray, parts);
                const Engine::Math::Vector3 size =
                    createdPart->GetProperty("Size").value<Engine::Math::Vector3>();
                const Engine::Math::Vector3 halfExtents{size.x * 0.5, size.y * 0.5, size.z * 0.5};

                Engine::Math::Vector3 newPosition = ray.Origin + (ray.Direction * 8.0);
                if (hitPart != nullptr && std::isfinite(hitDistance) && hitDistance > 0.0) {
                    const Engine::Math::Vector3 hitPoint = ray.Origin + (ray.Direction * hitDistance);
                    const Engine::Math::AABB hitAabb = Engine::Utils::BuildPartWorldAABB(hitPart);
                    const HitFace face = FindClosestHitFace(hitAabb, hitPoint);
                    const Engine::Math::Vector3 normal = AxisNormal(face);
                    const double pushOut = (face.Axis == 0) ? halfExtents.x : (face.Axis == 1) ? halfExtents.y : halfExtents.z;
                    newPosition = hitPoint + (normal * (pushOut + 1e-3));
                }

                createdPart->SetProperty("Position", QVariant::fromValue(newPosition));
            }
        }

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

bool StudioQuickActions::ShowContextMenu(
    QWidget& owner,
    const QPoint& globalPos,
    const std::shared_ptr<Engine::Core::Instance>& selected,
    const std::shared_ptr<Engine::Core::Instance>& insertParent
) const {
    if (selected == nullptr && insertParent == nullptr && clipboardPrototypes_.empty()) {
        return false;
    }

    QMenu menu(&owner);

    auto* insertMenu = menu.addMenu("Insert Object");
    PopulateInsertMenu(*insertMenu, insertParent);
    if (insertMenu->isEmpty()) {
        menu.removeAction(insertMenu->menuAction());
        delete insertMenu;
    }

    menu.addSeparator();

    QAction* copyAction = menu.addAction("Copy");
    StudioShortcutManager::ApplyToAction(*copyAction, StudioShortcutAction::Copy);
    copyAction->setEnabled(selected != nullptr && selected->IsInsertable());
    QObject::connect(copyAction, &QAction::triggered, &owner, [this]() { CopySelection(); });

    QAction* cutAction = menu.addAction("Cut");
    StudioShortcutManager::ApplyToAction(*cutAction, StudioShortcutAction::Cut);
    cutAction->setEnabled(selected != nullptr && selected->IsInsertable() && selected->GetParent() != nullptr);
    QObject::connect(cutAction, &QAction::triggered, &owner, [this]() { CutSelection(); });

    QAction* pasteAction = menu.addAction("Paste");
    StudioShortcutManager::ApplyToAction(*pasteAction, StudioShortcutAction::Paste);
    pasteAction->setEnabled(!clipboardPrototypes_.empty());
    QObject::connect(pasteAction, &QAction::triggered, &owner, [this]() { PasteSelectionToTopmostService(); });

    QAction* pasteIntoAction = menu.addAction("Paste Into Selection");
    StudioShortcutManager::ApplyToAction(*pasteIntoAction, StudioShortcutAction::PasteInto);
    pasteIntoAction->setEnabled(!clipboardPrototypes_.empty() && selected != nullptr);
    QObject::connect(pasteIntoAction, &QAction::triggered, &owner, [this]() { PasteSelectionIntoSelection(); });

    menu.addSeparator();

    QAction* groupAction = menu.addAction("Group");
    StudioShortcutManager::ApplyToAction(*groupAction, StudioShortcutAction::Group);
    {
        int groupableCount = 0;
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        for (const auto& inst : GetSelectedInstances(selectionService)) {
            if (inst == nullptr || inst->IsService() || !inst->IsInsertable() || inst->GetParent() == nullptr) {
                continue;
            }
            ++groupableCount;
            if (groupableCount >= 1) {
                break;
            }
        }
        groupAction->setEnabled(groupableCount >= 1);
    }
    QObject::connect(groupAction, &QAction::triggered, &owner, [this]() { GroupSelection(); });

    QAction* ungroupAction = menu.addAction("Ungroup");
    StudioShortcutManager::ApplyToAction(*ungroupAction, StudioShortcutAction::Ungroup);
    {
        bool canUngroup = false;
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        for (const auto& inst : GetSelectedInstances(selectionService)) {
            if (inst == nullptr || inst->GetParent() == nullptr) {
                continue;
            }
            const QString cls = inst->GetClassName();
            if (cls == "Model" || cls == "Folder") {
                canUngroup = true;
                break;
            }
        }
        ungroupAction->setEnabled(canUngroup);
    }
    QObject::connect(ungroupAction, &QAction::triggered, &owner, [this]() { UngroupSelection(); });

    QAction* duplicateAction = menu.addAction("Duplicate");
    StudioShortcutManager::ApplyToAction(*duplicateAction, StudioShortcutAction::Duplicate);
    {
        bool canDuplicate = false;
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        for (const auto& inst : GetSelectedInstances(selectionService)) {
            if (inst != nullptr && inst->IsInsertable() && inst->GetParent() != nullptr) {
                canDuplicate = true;
                break;
            }
        }
        duplicateAction->setEnabled(canDuplicate);
    }
    QObject::connect(duplicateAction, &QAction::triggered, &owner, [this]() { DuplicateSelection(); });

    QAction* deleteAction = menu.addAction("Delete");
    StudioShortcutManager::ApplyToAction(*deleteAction, StudioShortcutAction::Delete);
    {
        bool canDelete = false;
        const auto place = GetCurrentPlace();
        const auto selectionService = GetSelectionService(place);
        for (const auto& inst : GetSelectedInstances(selectionService)) {
            if (inst != nullptr && inst->IsInsertable() && inst->GetParent() != nullptr) {
                canDelete = true;
                break;
            }
        }
        deleteAction->setEnabled(canDelete);
    }
    QObject::connect(deleteAction, &QAction::triggered, &owner, [this]() { DeleteSelection(); });

    if (menu.actions().isEmpty()) {
        return false;
    }

    menu.exec(globalPos);
    return true;
}

std::shared_ptr<Engine::Core::Instance> StudioQuickActions::ResolveTopmostServicePasteParent(
    const std::shared_ptr<Engine::Core::Instance>& contextInstance
) const {
    const auto place = GetCurrentPlace();
    if (place == nullptr) {
        return nullptr;
    }

    std::shared_ptr<Engine::Core::Instance> serviceParent;
    auto current = contextInstance;
    while (current != nullptr) {
        if (current->IsService()) {
            serviceParent = current;
        }
        current = current->GetParent();
    }

    if (serviceParent != nullptr) {
        return serviceParent;
    }

    if (const auto workspace = place->FindService("Workspace"); workspace != nullptr) {
        return workspace;
    }
    return place->GetDataModel();
}

void StudioQuickActions::PasteToParent(
    const std::shared_ptr<Engine::Core::Instance>& parent,
    const QString& historyLabel
) const {
    try {
        if (clipboardPrototypes_.empty() || parent == nullptr) {
            return;
        }

        const auto place = GetCurrentPlace();
        const auto historyService = GetHistoryService(place);
        const auto selectionService = GetSelectionService(place);

        std::vector<std::shared_ptr<Engine::Core::Instance>> clones;
        clones.reserve(clipboardPrototypes_.size());

        auto recordAll = [&](const bool beginGroup) {
            if (historyService != nullptr && beginGroup) {
                historyService->BeginRecording(historyLabel);
            }
            try {
                for (const auto& proto : clipboardPrototypes_) {
                    const auto clone = CloneRecursive(proto);
                    if (clone == nullptr || !clone->CanParentTo(parent) || !parent->CanAcceptChild(clone)) {
                        continue;
                    }
                    clones.push_back(clone);
                    auto command = std::make_shared<Engine::Utils::ReparentCommand>(clone, parent);
                    if (historyService == nullptr) {
                        command->Do();
                    } else {
                        historyService->Record(command);
                    }
                }
            } catch (...) {
                if (historyService != nullptr && beginGroup) {
                    historyService->FinishRecording();
                }
                throw;
            }
            if (historyService != nullptr && beginGroup) {
                historyService->FinishRecording();
            }
        };

        if (historyService == nullptr) {
            recordAll(false);
        } else if (historyService->IsRecording()) {
            recordAll(false);
        } else {
            recordAll(true);
        }

        if (selectionService != nullptr && !clones.empty()) {
            selectionService->Set(clones);
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void StudioQuickActions::PasteToParents(
    const std::vector<std::shared_ptr<Engine::Core::Instance>>& parents,
    const QString& historyLabel
) const {
    try {
        if (clipboardPrototypes_.empty() || parents.empty()) {
            return;
        }

        std::vector<std::shared_ptr<Engine::Core::Instance>> uniqueParents;
        uniqueParents.reserve(parents.size());
        std::unordered_set<const Engine::Core::Instance*> seenParents;
        for (const auto& parent : parents) {
            if (parent == nullptr) {
                continue;
            }
            if (!seenParents.insert(parent.get()).second) {
                continue;
            }
            uniqueParents.push_back(parent);
        }
        if (uniqueParents.empty()) {
            return;
        }

        const auto place = GetCurrentPlace();
        const auto historyService = GetHistoryService(place);
        const auto selectionService = GetSelectionService(place);

        std::vector<std::shared_ptr<Engine::Core::Instance>> clones;

        auto recordAll = [&](const bool beginGroup) {
            if (historyService != nullptr && beginGroup) {
                historyService->BeginRecording(historyLabel);
            }
            try {
                for (const auto& parent : uniqueParents) {
                    if (parent == nullptr) {
                        continue;
                    }
                    for (const auto& proto : clipboardPrototypes_) {
                        const auto clone = CloneRecursive(proto);
                        if (clone == nullptr || !clone->CanParentTo(parent) || !parent->CanAcceptChild(clone)) {
                            continue;
                        }
                        clones.push_back(clone);
                        auto command = std::make_shared<Engine::Utils::ReparentCommand>(clone, parent);
                        if (historyService == nullptr) {
                            command->Do();
                        } else {
                            historyService->Record(command);
                        }
                    }
                }
            } catch (...) {
                if (historyService != nullptr && beginGroup) {
                    historyService->FinishRecording();
                }
                throw;
            }
            if (historyService != nullptr && beginGroup) {
                historyService->FinishRecording();
            }
        };

        if (historyService == nullptr) {
            recordAll(false);
        } else if (historyService->IsRecording()) {
            recordAll(false);
        } else {
            recordAll(true);
        }

        if (selectionService != nullptr && !clones.empty()) {
            selectionService->Set(clones);
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
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

Engine::Core::Viewport* StudioQuickActions::ResolveViewportFromObject(QObject* object) const {
    if (viewport_ == nullptr) {
        return nullptr;
    }

    QObject* current = object;
    while (current != nullptr) {
        if (current == viewport_) {
            return viewport_;
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
