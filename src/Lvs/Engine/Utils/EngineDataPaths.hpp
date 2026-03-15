#pragma once

#include <QString>

namespace Lvs::Engine::Utils::EngineDataPaths {

[[nodiscard]] QString RootDir();
[[nodiscard]] QString StudioConfigFile();
[[nodiscard]] QString LogsDir();
[[nodiscard]] QString CrashLogsDir();
[[nodiscard]] QString DefaultLocalAssetsDir();

} // namespace Lvs::Engine::Utils::EngineDataPaths

