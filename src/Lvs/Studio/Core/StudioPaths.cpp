#include "Lvs/Studio/Core/StudioPaths.hpp"

#include "Lvs/Engine/Utils/EngineDataPaths.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QDir>
#include <QFileInfo>

namespace Lvs::Studio::Core::StudioPaths {

QString LocalAssetsDir() {
    QString path;
    try {
        path = Settings::Get("LocalAssetsPath").toString().trimmed();
    } catch (...) {
        path.clear();
    }

    if (path.isEmpty()) {
        path = Engine::Utils::EngineDataPaths::DefaultLocalAssetsDir();
    }

    QDir().mkpath(path);
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

} // namespace Lvs::Studio::Core::StudioPaths

