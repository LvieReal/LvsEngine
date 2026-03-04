#pragma once

#include "Lvs/Engine/Core/EditorToolState.hpp"

#include <QHash>

class QAction;
class QActionGroup;
class QToolBar;

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Studio::Controllers {

class ToolbarController final {
public:
    ToolbarController(Engine::Core::Window& window, Engine::Core::EditorToolState& toolState);
    ~ToolbarController();

    void Build();
    void SetVisible(bool visible) const;

private:
    void AddToolAction(const QString& text, const QString& iconName, Engine::Core::Tool tool);
    void HandleToolAction(QAction* action, Engine::Core::Tool tool);
    void SetDefaultTool(Engine::Core::Tool tool);

    Engine::Core::Window& window_;
    Engine::Core::EditorToolState& toolState_;
    QToolBar* toolbar_{nullptr};
    QActionGroup* group_{nullptr};
    QAction* currentAction_{nullptr};
    QHash<int, QAction*> actions_;
};

} // namespace Lvs::Studio::Controllers
