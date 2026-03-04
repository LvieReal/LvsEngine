#pragma once

#include "Lvs/Engine/Context.hpp"

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Engine::Bootstrap {

EngineContextPtr Run(Core::Window& window);

} // namespace Lvs::Engine::Bootstrap
