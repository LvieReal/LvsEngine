#include "Lvs/Studio/Configuration.hpp"
#include "Lvs/AppInfo.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

namespace Lvs::Studio::Configuration {

QString GetName() {
    return AppInfo::GetName();
}

QString GetFullName() {
    return QString("%1 %2").arg(GetName()).arg(GetYear());
}

QString GetFullCreatorName() {
    return AppInfo::GetCreatorName();
}

QString GetVersion() {
    return AppInfo::GetVersion();
}

int GetYear() {
    return AppInfo::GetYear();
}

QString GetStudioWindowName() {
    return AppInfo::GetStudioWindowName();
}

QString GetLogoPathPNG() {
    return Engine::Utils::SourcePath::GetResourcePath(AppInfo::GetLogoPng());
}

QString GetLogoPathICO() {
    return Engine::Utils::SourcePath::GetResourcePath(AppInfo::GetLogoIco());
}

} // namespace Lvs::Studio::Configuration
