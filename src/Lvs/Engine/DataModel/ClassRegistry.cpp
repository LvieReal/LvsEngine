#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <QHash>

#include <stdexcept>

namespace Lvs::Engine::DataModel::ClassRegistry {

namespace {
QHash<QString, ClassInfo>& Registry() {
    static QHash<QString, ClassInfo> registry;
    return registry;
}

QMap<QString, QVector<ClassInfo>>& CategoryMap() {
    static QMap<QString, QVector<ClassInfo>> categoryMap;
    return categoryMap;
}
} // namespace

void RegisterClass(const ClassInfo& classInfo) {
    Registry().insert(classInfo.Name, classInfo);
    CategoryMap()[classInfo.Category].push_back(classInfo);
}

std::shared_ptr<Core::Instance> CreateInstance(const QString& name) {
    if (!Registry().contains(name)) {
        throw std::runtime_error(QString("Class not registered: %1").arg(name).toStdString());
    }
    return Registry().value(name).Factory();
}

QVector<ClassInfo> GetClasses() {
    QVector<ClassInfo> classes;
    classes.reserve(Registry().size());
    for (auto it = Registry().cbegin(); it != Registry().cend(); ++it) {
        classes.push_back(it.value());
    }
    return classes;
}

QMap<QString, QVector<ClassInfo>> GetClassesByCategory() {
    return CategoryMap();
}

QVector<ClassInfo> GetDerivedClasses(const QString& baseName) {
    QVector<ClassInfo> derived;
    for (auto it = Registry().cbegin(); it != Registry().cend(); ++it) {
        if (it.value().BaseClass == baseName) {
            derived.push_back(it.value());
        }
    }
    return derived;
}

} // namespace Lvs::Engine::DataModel::ClassRegistry
