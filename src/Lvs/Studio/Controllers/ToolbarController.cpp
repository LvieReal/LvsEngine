#include "Lvs/Studio/Controllers/ToolbarController.hpp"

#include "Lvs/Studio/Core/Window.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"

#include <QAction>
#include <QActionGroup>
#include <QSize>
#include <QToolBar>
#include <Qt>

namespace Lvs::Studio::Controllers {

ToolbarController::ToolbarController(Engine::Core::Window& window, Engine::Core::EditorToolState& toolState)
    : window_(window),
      toolState_(toolState) {
}

ToolbarController::~ToolbarController() {
    toolChangedConnection_.Disconnect();
}

void ToolbarController::Build() {
    toolChangedConnection_.Disconnect();

    group_ = new QActionGroup(&window_);
    group_->setExclusive(false);

    toolbar_ = new QToolBar("Edit Modes", &window_);
    toolbar_->setObjectName("EditModes");
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);
    toolbar_->setIconSize(QSize(16, 16));
    toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

    AddToolAction("Select", "cursor.png", Engine::Core::Tool::SelectTool);
    AddToolAction("Move", "shape_move_front.png", Engine::Core::Tool::MoveTool);
    AddToolAction("Size", "shape_handles.png", Engine::Core::Tool::SizeTool);

    window_.addToolBar(Qt::TopToolBarArea, toolbar_);
    toolChangedConnection_ = toolState_.ToolChanged.Connect([this](const Engine::Core::Tool tool) {
        SyncActionStates(tool);
    });
    SetDefaultTool(Engine::Core::Tool::SelectTool);
}

void ToolbarController::SetVisible(const bool visible) const {
    if (toolbar_ != nullptr) {
        toolbar_->setVisible(visible);
    }
}

void ToolbarController::AddToolAction(const QString& text, const QString& iconName, const Engine::Core::Tool tool) {
    if (toolbar_ == nullptr || group_ == nullptr) {
        return;
    }

    auto* action = new QAction(Core::GetIconPackManager().GetIcon(iconName), text, &window_);
    action->setCheckable(true);
    action->setToolTip(text);
    action->setIconText(text);
    group_->addAction(action);
    toolbar_->addAction(action);
    QObject::connect(action, &QAction::triggered, &window_, [this, action, tool](const bool) {
        HandleToolAction(action, tool);
    });
    actions_.insert(static_cast<int>(tool), action);
}

void ToolbarController::HandleToolAction(QAction* action, const Engine::Core::Tool tool) {
    if (action == nullptr) {
        return;
    }

    if (currentAction_ == action) {
        action->setChecked(false);
        currentAction_ = nullptr;
        toolState_.SetTool(Engine::Core::Tool::NoneTool);
        return;
    }

    if (currentAction_ != nullptr) {
        currentAction_->setChecked(false);
    }

    action->setChecked(true);
    currentAction_ = action;
    toolState_.SetTool(tool);
}

void ToolbarController::ActivateTool(const Engine::Core::Tool tool) {
    SyncActionStates(tool);
    toolState_.SetTool(tool);
}

void ToolbarController::SyncActionStates(const Engine::Core::Tool tool) {
    QAction* action = actions_.value(static_cast<int>(tool), nullptr);
    if (tool == Engine::Core::Tool::NoneTool || action == nullptr) {
        if (currentAction_ != nullptr) {
            currentAction_->setChecked(false);
        }
        currentAction_ = nullptr;
        return;
    }

    if (currentAction_ != nullptr && currentAction_ != action) {
        currentAction_->setChecked(false);
    }
    action->setChecked(true);
    currentAction_ = action;
}

void ToolbarController::SetDefaultTool(const Engine::Core::Tool tool) {
    if (actions_.value(static_cast<int>(tool), nullptr) == nullptr) {
        return;
    }
    SyncActionStates(tool);
    toolState_.SetTool(tool);
}

} // namespace Lvs::Studio::Controllers
