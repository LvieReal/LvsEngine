#pragma once

#include <QString>
#include <QStringList>

namespace Lvs::AppInfo {

QString GetName();
QString GetStudioWindowName();
QString GetAppWindowName();
QString GetCreatorName();
QString GetVersion();
int GetYear();
QString GetLogoPng();
QString GetLogoIco();
QStringList GetCredits();

} // namespace Lvs::AppInfo
