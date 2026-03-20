#pragma once

#include "Lvs/Engine/Core/Types.hpp"

namespace Lvs::Engine::Utils::EngineDataPaths {

[[nodiscard]] Core::String RootDir();
[[nodiscard]] Core::String StudioConfigFile();
[[nodiscard]] Core::String LogsDir();
[[nodiscard]] Core::String CrashLogsDir();
[[nodiscard]] Core::String DefaultLocalAssetsDir();

} // namespace Lvs::Engine::Utils::EngineDataPaths
