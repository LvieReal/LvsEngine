#pragma once

#include "Lvs/Launcher/BuildType.hpp"

namespace Lvs::Launcher {

int Run(int argc, char* argv[], BuildType buildType, const char* appName, const char* appIconPath);

} // namespace Lvs::Launcher
