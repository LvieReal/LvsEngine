#pragma once

#include <QString>
#include <QStringList>

#include <memory>

class QPixmap;
class QIcon;

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Studio::Core {

class IconPackManager final {
public:
    IconPackManager();
    ~IconPackManager() = default;

    void SetActivePack(const QString& packName);
    [[nodiscard]] QString GetActivePackName() const;
    [[nodiscard]] QStringList GetAvailablePacks() const;
    [[nodiscard]] QPixmap GetPixmapForInstance(const std::shared_ptr<Engine::Core::Instance>& instance);
    [[nodiscard]] QIcon GetIcon(const QString& iconName);

private:
    [[nodiscard]] QStringList GetPackRoots() const;
    [[nodiscard]] QString ResolvePackPath(const QString& packName) const;
    [[nodiscard]] QPixmap LoadPixmap(const QString& iconName);

    QString activePackName_;
    QString packPath_;
    bool missingPackWarned_{false};
};

IconPackManager& GetIconPackManager();

} // namespace Lvs::Studio::Core
