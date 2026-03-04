#include "Lvs/Studio/Configuration.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace Lvs::Studio::Configuration {

namespace {

struct AppInfo {
    QString Name{""};
    QString StudioWindowName{""};
    QString AppWindowName{""};
    QString CreatorName{""};
    QString Version{"0.0.0"};
    int Year{0};
    QString LogoPng{"Logo/LogoShape.png"};
    QString LogoIco{"Logo/LogoShape.ico"};
};

const AppInfo& LoadInfo() {
    static const AppInfo info = []() {
        AppInfo out;
        QFile file(":/config/AppInfo.json");
        if (!file.open(QIODevice::ReadOnly)) {
            file.setFileName(Engine::Utils::SourcePath::GetSourcePath("config/AppInfo.json"));
            if (!file.open(QIODevice::ReadOnly)) {
                return out;
            }
        }

        const auto doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isObject()) {
            return out;
        }

        const QJsonObject obj = doc.object();
        out.Name = obj.value("name").toString(out.Name);
        out.StudioWindowName = obj.value("studio_window_name").toString(out.StudioWindowName);
        out.AppWindowName = obj.value("app_window_name").toString(out.AppWindowName);
        out.CreatorName = obj.value("creator_name").toString(out.CreatorName);
        out.Version = obj.value("version").toString(out.Version);
        out.Year = obj.value("year").toInt(out.Year);
        out.LogoPng = obj.value("logo_png").toString(out.LogoPng);
        out.LogoIco = obj.value("logo_ico").toString(out.LogoIco);
        return out;
    }();
    return info;
}

} // namespace

QString GetName() {
    return LoadInfo().Name;
}

QString GetFullName() {
    return QString("%1 %2").arg(GetName()).arg(GetYear());
}

QString GetFullCreatorName() {
    return LoadInfo().CreatorName;
}

QString GetVersion() {
    return LoadInfo().Version;
}

int GetYear() {
    return LoadInfo().Year;
}

QString GetStudioWindowName() {
    return LoadInfo().StudioWindowName;
}

QString GetAppWindowName() {
    return LoadInfo().AppWindowName;
}

QString GetLogoPathPNG() {
    return Engine::Utils::SourcePath::GetResourcePath(LoadInfo().LogoPng);
}

QString GetLogoPathICO() {
    return Engine::Utils::SourcePath::GetResourcePath(LoadInfo().LogoIco);
}

} // namespace Lvs::Studio::Configuration
