#include "Lvs/AppInfo.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"
#include "Lvs/Engine/Utils/Toml.hpp"

#include <QFile>

namespace Lvs::AppInfo {

namespace {

struct AppInfoData {
    QString Name{"Lvs Studio"};
    QString StudioWindowName{"Lvs Studio"};
    QString AppWindowName{"Unnamed"};
    QString CreatorName{"Unknown"};
    QString Version{"0.0.0"};
    int Year{2026};
    QString LogoPng{"Logo/LogoShape.png"};
    QString LogoIco{"Logo/LogoShape.ico"};
    QStringList Credits{};
};

const AppInfoData& LoadInfo() {
    static const AppInfoData info = []() {
        AppInfoData out;
        QFile file(":/config/AppInfo.toml");
        if (!file.open(QIODevice::ReadOnly)) {
            file.setFileName(QString::fromUtf8(Engine::Utils::SourcePath::GetSourcePath("config/AppInfo.toml").c_str()));
            if (!file.open(QIODevice::ReadOnly)) {
                return out;
            }
        }

        const QByteArray data = file.readAll();
        file.close();
        const std::string text = data.toStdString();
        Engine::Utils::Toml::Document doc;
        try {
            doc = Engine::Utils::Toml::Parse(text);
        } catch (...) {
            return out;
        }

        const auto* nameV = Engine::Utils::Toml::FindValue(doc.Root, "name");
        const auto* studioNameV = Engine::Utils::Toml::FindValue(doc.Root, "studio_window_name");
        const auto* appNameV = Engine::Utils::Toml::FindValue(doc.Root, "app_window_name");
        const auto* creatorV = Engine::Utils::Toml::FindValue(doc.Root, "creator_name");
        const auto* versionV = Engine::Utils::Toml::FindValue(doc.Root, "version");
        const auto* yearV = Engine::Utils::Toml::FindValue(doc.Root, "year");
        const auto* logoPngV = Engine::Utils::Toml::FindValue(doc.Root, "logo_png");
        const auto* logoIcoV = Engine::Utils::Toml::FindValue(doc.Root, "logo_ico");
        const auto* creditsV = Engine::Utils::Toml::FindValue(doc.Root, "credits");

        if (nameV && nameV->IsString()) out.Name = QString::fromUtf8(nameV->AsString().c_str());
        if (studioNameV && studioNameV->IsString()) out.StudioWindowName = QString::fromUtf8(studioNameV->AsString().c_str());
        if (appNameV && appNameV->IsString()) out.AppWindowName = QString::fromUtf8(appNameV->AsString().c_str());
        if (creatorV && creatorV->IsString()) out.CreatorName = QString::fromUtf8(creatorV->AsString().c_str());
        if (versionV && versionV->IsString()) out.Version = QString::fromUtf8(versionV->AsString().c_str());
        if (yearV && yearV->IsInt()) out.Year = static_cast<int>(yearV->AsInt(out.Year));
        if (logoPngV && logoPngV->IsString()) out.LogoPng = QString::fromUtf8(logoPngV->AsString().c_str());
        if (logoIcoV && logoIcoV->IsString()) out.LogoIco = QString::fromUtf8(logoIcoV->AsString().c_str());

        if (creditsV && creditsV->IsArray()) {
            for (const auto& entry : creditsV->AsArray()) {
                if (entry.IsString()) {
                    out.Credits.push_back(QString::fromUtf8(entry.AsString().c_str()));
                }
            }
        }
        return out;
    }();
    return info;
}

} // namespace

QString GetName() {
    return LoadInfo().Name;
}

QString GetStudioWindowName() {
    return LoadInfo().StudioWindowName;
}

QString GetAppWindowName() {
    return LoadInfo().AppWindowName;
}

QString GetCreatorName() {
    return LoadInfo().CreatorName;
}

QString GetVersion() {
    return LoadInfo().Version;
}

int GetYear() {
    return LoadInfo().Year;
}

QString GetLogoPng() {
    return LoadInfo().LogoPng;
}

QString GetLogoIco() {
    return LoadInfo().LogoIco;
}

QStringList GetCredits() {
    return LoadInfo().Credits;
}

} // namespace Lvs::AppInfo
