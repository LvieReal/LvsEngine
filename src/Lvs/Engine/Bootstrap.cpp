#include "Lvs/Engine/Bootstrap.hpp"

#include "Lvs/Engine/Reflection/ReflectionSystem.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include <memory>

namespace Lvs::Engine::Bootstrap {

EngineContextPtr Run() {
    Reflection::EnsureInitialized();
    auto context = std::make_shared<EngineContext>();
    context->RenderContext = Rendering::CreateRenderContext();
    context->RenderContext->Initialize(1U, 1U);
    return context;
}

} // namespace Lvs::Engine::Bootstrap
