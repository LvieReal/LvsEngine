#pragma once

#include "Lvs/Engine/Core/Types.hpp"

namespace Lvs::Engine::Utils::SourcePath {

inline constexpr auto CORE_PATH_FORMAT = "core://";
inline constexpr auto LOCAL_PATH_FORMAT = "local://";

Core::String GetExecutablePath();
Core::String GetSourcePath(const Core::String& path);
Core::String GetResourcePath(const Core::String& path);

Core::String OsToCorePath(const Core::String& path);
Core::String OsToLocalPath(const Core::String& path);
Core::String CorePathToOs(const Core::String& path);
Core::String LocalPathToOs(const Core::String& path);
Core::String ToOsPath(const Core::String& path);

} // namespace Lvs::Engine::Utils::SourcePath
