#pragma once

#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"

#include <memory>

namespace Lvs::Engine {

struct EngineContext {
    EngineContext() = default;
    ~EngineContext();

    std::unique_ptr<Rendering::IRenderContext> RenderContext;
    std::unique_ptr<DataModel::PlaceManager> PlaceManager;
    std::unique_ptr<Core::EditorToolState> EditorToolState;
};

using EngineContextPtr = std::shared_ptr<EngineContext>;

} // namespace Lvs::Engine
