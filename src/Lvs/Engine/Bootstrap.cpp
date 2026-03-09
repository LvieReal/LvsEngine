#include "Lvs/Engine/Bootstrap.hpp"

#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include <memory>
#include <algorithm>

namespace Lvs::Engine::Bootstrap {

EngineContextPtr Run(Core::Window& window) {
    auto context = std::make_shared<EngineContext>();
    context->RenderContext = Rendering::CreateRenderContext();
    context->RenderContext->Initialize(
        static_cast<Rendering::RHI::u32>(std::max(window.width(), 1)),
        static_cast<Rendering::RHI::u32>(std::max(window.height(), 1))
    );
    static_cast<void>(window);
    return context;
}

} // namespace Lvs::Engine::Bootstrap
