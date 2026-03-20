#include "Lvs/Studio/Configuration.hpp"
#include "Lvs/AppInfo.hpp"
#include "Lvs/Engine/Core/QtBridge.hpp"
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
    return Engine::Core::QtBridge::ToQString(
        Engine::Utils::SourcePath::GetResourcePath(Engine::Core::QtBridge::ToStdString(AppInfo::GetLogoPng()))
    );
}

QString GetLogoPathICO() {
    return Engine::Core::QtBridge::ToQString(
        Engine::Utils::SourcePath::GetResourcePath(Engine::Core::QtBridge::ToStdString(AppInfo::GetLogoIco()))
    );
}

} // namespace Lvs::Studio::Configuration
