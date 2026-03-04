#include "Lvs/App/Configuration.hpp"
#include "Lvs/AppInfo.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

namespace Lvs::App::Configuration {

QString GetWindowName() {
    return AppInfo::GetAppWindowName();
}

QString GetLogoPathPNG() {
    return Engine::Utils::SourcePath::GetResourcePath(AppInfo::GetLogoPng());
}

QString GetLogoPathICO() {
    return Engine::Utils::SourcePath::GetResourcePath(AppInfo::GetLogoIco());
}

} // namespace Lvs::App::Configuration
