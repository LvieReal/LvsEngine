#include "Lvs/Studio/Core/IconPackManager.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Qt/QtBridge.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QtGlobal>

#include <algorithm>

namespace Lvs::Studio::Core {

namespace {
constexpr auto DEFAULT_PACK_NAME = "famfamfam-silk";
constexpr auto DEFAULT_ICON_NAME = "shape_square.png";
constexpr auto DEFAULT_SERVICE_ICON_NAME = "shape_square.png";
constexpr int ICON_SIZE = 14;

const QHash<QString, QString> CLASS_ICON_MAP = {
    {"Workspace", "world.png"},
    {"Lighting", "lightbulb.png"},
    {"ChangeHistoryService", "arrow_undo.png"},
    {"Camera", "camera.png"},
    {"DirectionalLight", "weather_sun.png"},
    {"Skybox", "picture.png"},
    {"PostEffects", "color_wheel.png"},
    {"Part", "brick.png"},
    {"MeshPart", "box.png"},
    {"Model", "bricks.png"},
    {"Folder", "folder.png"}
};

QHash<QString, QPixmap>& PixmapCache() {
    static QHash<QString, QPixmap> cache;
    return cache;
}
} // namespace

IconPackManager::IconPackManager() {
    activePackName_ = Settings::Get("StudioIconPack").toString();
    packPath_ = ResolvePackPath(activePackName_);

    Settings::Changed("StudioIconPack", [this](const QVariant& value) {
        SetActivePack(value.toString());
    });
}

void IconPackManager::SetActivePack(const QString& packName) {
    activePackName_ = packName.trimmed();
    packPath_ = ResolvePackPath(activePackName_);
    PixmapCache().clear();
    missingPackWarned_ = false;
}

QString IconPackManager::GetActivePackName() const {
    return activePackName_;
}

QStringList IconPackManager::GetAvailablePacks() const {
    QStringList result;
    QSet<QString> seen;
    for (const QString& root : GetPackRoots()) {
        const QDir dir(root);
        if (!dir.exists()) {
            continue;
        }
        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& entry : entries) {
            if (!seen.contains(entry.fileName())) {
                seen.insert(entry.fileName());
                result.push_back(entry.fileName());
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const QString& left, const QString& right) {
        return left.toCaseFolded() < right.toCaseFolded();
    });
    return result;
}

QPixmap IconPackManager::GetPixmapForInstance(const std::shared_ptr<Engine::Core::Instance>& instance) {
    if (instance == nullptr) {
        return {};
    }

    QString iconName = CLASS_ICON_MAP.value(QString::fromUtf8(instance->GetClassName().c_str()));
    if (iconName.isEmpty()) {
        iconName = instance->IsService() ? DEFAULT_SERVICE_ICON_NAME : DEFAULT_ICON_NAME;
    }

    QPixmap pixmap = LoadPixmap(iconName);
    if (!pixmap.isNull()) {
        return pixmap;
    }
    if (iconName != DEFAULT_ICON_NAME) {
        pixmap = LoadPixmap(DEFAULT_ICON_NAME);
    }
    return pixmap;
}

QIcon IconPackManager::GetIcon(const QString& iconName) {
    QPixmap pixmap = LoadPixmap(iconName);
    if (pixmap.isNull() && iconName != DEFAULT_ICON_NAME) {
        pixmap = LoadPixmap(DEFAULT_ICON_NAME);
    }
    return QIcon(pixmap);
}

QString IconPackManager::GetIconPath(const QString& iconName) const {
    if (packPath_.isEmpty()) {
        return {};
    }
    const QString name = iconName.trimmed();
    if (name.isEmpty()) {
        return {};
    }
    return QDir(packPath_).filePath(name);
}

QStringList IconPackManager::GetPackRoots() const {
    return {
        Engine::Core::QtBridge::ToQString(Engine::Utils::SourcePath::GetResourcePath("IconPacks")),
        Engine::Core::QtBridge::ToQString(Engine::Utils::SourcePath::GetSourcePath("content/IconPacks"))
    };
}

QString IconPackManager::ResolvePackPath(const QString& packName) const {
    const QString normalized = packName.trimmed();
    if (!normalized.isEmpty()) {
        for (const QString& root : GetPackRoots()) {
            const QString candidate = QDir(root).filePath(normalized);
            if (QFileInfo::exists(candidate) && QFileInfo(candidate).isDir()) {
                return candidate;
            }
        }
    }

    for (const QString& root : GetPackRoots()) {
        const QString fallback = QDir(root).filePath(DEFAULT_PACK_NAME);
        if (QFileInfo::exists(fallback) && QFileInfo(fallback).isDir()) {
            return fallback;
        }
    }
    return {};
}

QPixmap IconPackManager::LoadPixmap(const QString& iconName) {
    if (packPath_.isEmpty()) {
        if (!missingPackWarned_) {
            qWarning("No icon pack found under content/IconPacks.");
            missingPackWarned_ = true;
        }
        return {};
    }

    const QString iconPath = QDir(packPath_).filePath(iconName);
    if (PixmapCache().contains(iconPath)) {
        return PixmapCache().value(iconPath);
    }

    QPixmap pixmap;
    if (QFileInfo::exists(iconPath) && QFileInfo(iconPath).isFile()) {
        pixmap = QPixmap(iconPath)
                     .scaled(
                         QSize(ICON_SIZE, ICON_SIZE),
                         Qt::KeepAspectRatio,
                         Qt::SmoothTransformation
                     );
    }

    PixmapCache().insert(iconPath, pixmap);
    return pixmap;
}

IconPackManager& GetIconPackManager() {
    static IconPackManager manager;
    return manager;
}

} // namespace Lvs::Studio::Core
