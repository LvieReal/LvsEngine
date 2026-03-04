#include "Lvs/Engine/Bootstrap.hpp"

#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"

#include <memory>

namespace Lvs::Engine::Bootstrap {

EngineContextPtr Run(Core::Window& window) {
    auto context = std::make_shared<EngineContext>();
    context->Vulkan = std::make_unique<Rendering::Vulkan::VulkanContext>();
    context->Vulkan->Initialize();
    static_cast<void>(window);
    return context;
}

} // namespace Lvs::Engine::Bootstrap
