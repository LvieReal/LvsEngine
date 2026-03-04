#pragma once

#include <QString>

namespace Lvs::Studio::Configuration {

QString GetName();
QString GetFullName();
QString GetFullCreatorName();
QString GetVersion();
int GetYear();
QString GetStudioWindowName();
QString GetAppWindowName();
QString GetLogoPathPNG();
QString GetLogoPathICO();

} // namespace Lvs::Studio::Configuration
