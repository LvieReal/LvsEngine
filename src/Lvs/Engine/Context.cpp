#include "Lvs/Engine/Context.hpp"

#include "Lvs/Engine/DataModel/PlaceManager.hpp"
#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Studio/Controllers/ToolbarController.hpp"
#include "Lvs/Studio/Controllers/TopBarController.hpp"
#include "Lvs/Studio/Core/DockManager.hpp"
#include "Lvs/Studio/Core/HistoryShortcuts.hpp"
#include "Lvs/Studio/Core/ViewportManager.hpp"

namespace Lvs::Engine {

EngineContext::~EngineContext() = default;

} // namespace Lvs::Engine
