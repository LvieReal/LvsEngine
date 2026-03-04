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

    Utils::Signal<Tool> ToolChanged;

private:
    Tool activeTool_{Tool::SelectTool};
};

} // namespace Lvs::Engine::Core

