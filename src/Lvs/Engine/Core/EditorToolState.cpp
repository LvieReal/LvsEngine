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

void EditorToolState::SetLocalSpace(const bool enabled) {
    if (localSpace_ == enabled) {
        return;
    }
    localSpace_ = enabled;
    LocalSpaceChanged.Fire(localSpace_);
}

void EditorToolState::ToggleLocalSpace() {
    SetLocalSpace(!localSpace_);
}

bool EditorToolState::GetLocalSpace() const {
    return localSpace_;
}

} // namespace Lvs::Engine::Core
