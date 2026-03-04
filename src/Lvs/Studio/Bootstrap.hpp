#pragma once

#include "Lvs/Engine/Context.hpp"

class QApplication;

namespace Lvs::Engine::Core {
class Window;
}

namespace Lvs::Studio::Bootstrap {

void Run(QApplication& app, Engine::Core::Window& window, const Engine::EngineContextPtr& context);

} // namespace Lvs::Studio::Bootstrap
