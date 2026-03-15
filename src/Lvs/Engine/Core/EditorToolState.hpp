#pragma once

#include "Lvs/Engine/Utils/Signal.hpp"

namespace Lvs::Engine::Core {

enum class Tool {
    NoneTool = 0,
    SelectTool = 1,
    MoveTool = 2,
    SizeTool = 3
};

class EditorToolState final {
public:
    void SetTool(Tool tool);
    [[nodiscard]] Tool GetActiveTool() const;

    void SetLocalSpace(bool enabled);
    void ToggleLocalSpace();
    [[nodiscard]] bool GetLocalSpace() const;

    Utils::Signal<Tool> ToolChanged;
    Utils::Signal<bool> LocalSpaceChanged;

private:
    Tool activeTool_{Tool::SelectTool};
    bool localSpace_{false};
};

} // namespace Lvs::Engine::Core
