#include "Lvs/App/Configuration.hpp"
#include "Lvs/AppInfo.hpp"
#include "Lvs/Engine/Core/QtBridge.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

namespace Lvs::App::Configuration {

QString GetWindowName() {
    return AppInfo::GetAppWindowName();
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

} // namespace Lvs::App::Configuration
