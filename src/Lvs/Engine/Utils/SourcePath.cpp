#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstring>
#include <stdexcept>

namespace Lvs::Engine::Utils::SourcePath {

namespace {

QString Normalize(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString FindAnchorDir() {
    QStringList roots = {QDir::currentPath()};
    if (QCoreApplication::instance() != nullptr) {
        roots.push_back(QCoreApplication::applicationDirPath());
    }

    for (const QString& root : roots) {
        QDir dir(root);
        for (int i = 0; i < 6; ++i) {
            if (QFileInfo::exists(dir.filePath("src/Lvs/Engine/Content"))) {
                return dir.absolutePath();
            }
            if (QFileInfo::exists(dir.filePath("Lvs/Engine/Content"))) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    return QDir::currentPath();
}

QString ResourceRoot() {
    static const QString root = []() {
        const QString anchor = FindAnchorDir();
        const QString devPath = QDir(anchor).filePath("src/Lvs/Engine/Content");
        if (QFileInfo::exists(devPath)) {
            return Normalize(devPath);
        }
        return Normalize(QDir(anchor).filePath("Lvs/Engine/Content"));
    }();
    return root;
}

QString SourceRoot() {
    static const QString root = Normalize(FindAnchorDir());
    return root;
}

QString RelativeNormalized(const QString& path) {
    return QDir::cleanPath(path).replace('\\', '/');
}

} // namespace

QString GetExecutablePath() {
    return QCoreApplication::applicationDirPath();
}

QString GetSourcePath(const QString& path) {
    return Normalize(QDir(SourceRoot()).filePath(path));
}

QString GetResourcePath(const QString& path) {
    return Normalize(QDir(ResourceRoot()).filePath(path));
}

QString OsToCorePath(const QString& path) {
    const QString base = Normalize(GetResourcePath({}));
    const QString target = Normalize(path);

    if (!target.startsWith(base, Qt::CaseInsensitive)) {
        throw std::runtime_error(QString("Path '%1' is not inside core resources").arg(path).toStdString());
    }

    QString rel = QDir(base).relativeFilePath(target);
    rel = RelativeNormalized(rel);
    return QString("%1%2").arg(CORE_PATH_FORMAT, rel);
}

QString OsToLocalPath(const QString& path) {
    const QString base = Normalize(GetSourcePath({}));
    const QString target = Normalize(path);

    if (!target.startsWith(base, Qt::CaseInsensitive)) {
        throw std::runtime_error(QString("Path '%1' is not inside local source").arg(path).toStdString());
    }

    QString rel = QDir(base).relativeFilePath(target);
    rel = RelativeNormalized(rel);
    return QString("%1%2").arg(LOCAL_PATH_FORMAT, rel);
}

QString CorePathToOs(const QString& path) {
    if (!path.startsWith(CORE_PATH_FORMAT)) {
        throw std::runtime_error("Invalid core path");
    }
    const QString rel = path.mid(static_cast<int>(strlen(CORE_PATH_FORMAT)));
    return Normalize(GetResourcePath(rel));
}

QString LocalPathToOs(const QString& path) {
    if (!path.startsWith(LOCAL_PATH_FORMAT)) {
        throw std::runtime_error("Invalid local path");
    }
    const QString rel = path.mid(static_cast<int>(strlen(LOCAL_PATH_FORMAT)));
    return Normalize(GetSourcePath(rel));
}

QString ToOsPath(const QString& path) {
    if (path.isEmpty()) {
        return path;
    }
    if (path.startsWith(CORE_PATH_FORMAT)) {
        return CorePathToOs(path);
    }
    if (path.startsWith(LOCAL_PATH_FORMAT)) {
        return LocalPathToOs(path);
    }
    return Normalize(path);
}

} // namespace Lvs::Engine::Utils::SourcePath
