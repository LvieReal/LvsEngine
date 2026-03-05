#pragma once

#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Studio/Controllers/ToolbarController.hpp"
#include "Lvs/Studio/Controllers/TopBarController.hpp"
#include "Lvs/Studio/Core/DockManager.hpp"
#include "Lvs/Studio/Core/HistoryShortcuts.hpp"
#include "Lvs/Studio/Core/StudioQuickActions.hpp"
#include "Lvs/Studio/Core/ViewportManager.hpp"

#include <memory>

namespace Lvs::Engine {

struct EngineContext {
    EngineContext() = default;
    ~EngineContext();

    std::unique_ptr<Rendering::Vulkan::VulkanContext> Vulkan;
    std::unique_ptr<DataModel::PlaceManager> PlaceManager;
    std::unique_ptr<Studio::Core::DockManager> DockManager;
    std::unique_ptr<Studio::Core::ViewportManager> ViewportManager;
    std::unique_ptr<Studio::Controllers::TopBarController> TopBarController;
    std::unique_ptr<Studio::Controllers::ToolbarController> ToolbarController;
    std::unique_ptr<Studio::Core::HistoryShortcuts> HistoryShortcuts;
    std::unique_ptr<Studio::Core::StudioQuickActions> StudioQuickActions;
    std::unique_ptr<Core::EditorToolState> EditorToolState;
};

using EngineContextPtr = std::shared_ptr<EngineContext>;

} // namespace Lvs::Engine
