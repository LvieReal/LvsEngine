#pragma once

#include <QString>

namespace Lvs::Studio::Configuration {

QString GetName();
QString GetFullName();
QString GetFullCreatorName();
QString GetVersion();
int GetYear();
QString GetStudioWindowName();
QString GetLogoPathPNG();
QString GetLogoPathICO();
QStringList GetCredits();

} // namespace Lvs::Studio::Configuration
