#include "Lvs/Engine/Utils/EngineDataPaths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace Lvs::Engine::Utils::EngineDataPaths {

namespace {

QString EnsureDir(const QString& path) {
    const QString normalized = QDir::cleanPath(path);
    if (normalized.isEmpty()) {
        return normalized;
    }

    QDir dir(normalized);
    if (!dir.exists()) {
        QDir().mkpath(normalized);
    }
    return QFileInfo(normalized).absoluteFilePath();
}

QString Join(const QString& base, const QString& child) {
    return QDir(base).filePath(child);
}

} // namespace

QString RootDir() {
#if defined(_WIN32)
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    return EnsureDir(Join(base, "LvsEngine"));
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    return EnsureDir(Join(base, "LvsEngine"));
#endif
}

QString StudioConfigFile() {
    return Join(RootDir(), "studioConfig.json");
}

QString LogsDir() {
    return EnsureDir(Join(RootDir(), "logs"));
}

QString CrashLogsDir() {
    return EnsureDir(Join(RootDir(), "crash_logs"));
}

QString DefaultLocalAssetsDir() {
    return EnsureDir(Join(RootDir(), "assets"));
}

} // namespace Lvs::Engine::Utils::EngineDataPaths

