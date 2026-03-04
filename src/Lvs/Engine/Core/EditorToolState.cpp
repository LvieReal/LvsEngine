#include "Lvs/Engine/Core/EditorToolState.hpp"

namespace Lvs::Engine::Core {

void EditorToolState::SetTool(const Tool tool) {
    if (activeTool_ == tool) {
        return;
    }
    activeTool_ = tool;
    ToolChanged.Fire(tool);
}

Tool EditorToolState::GetActiveTool() const {
    return activeTool_;
}

} // namespace Lvs::Engine::Core

