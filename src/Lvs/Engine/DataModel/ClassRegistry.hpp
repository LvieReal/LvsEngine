#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

#include <QMap>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace Lvs::Engine::DataModel::ClassRegistry {

struct ClassInfo {
    QString Name;
    QString Category;
    QString BaseClass;
    std::function<std::shared_ptr<Core::Instance>()> Factory;
};

void RegisterClass(const ClassInfo& classInfo);
std::shared_ptr<Core::Instance> CreateInstance(const QString& name);
QVector<ClassInfo> GetClasses();
QMap<QString, QVector<ClassInfo>> GetClassesByCategory();
QVector<ClassInfo> GetDerivedClasses(const QString& baseName);

template <typename T>
void RegisterClass(
    const QString& name,
    const QString& category = "General",
    const QString& baseClass = {}
) {
    RegisterClass(ClassInfo{
        .Name = name,
        .Category = category,
        .BaseClass = baseClass,
        .Factory = []() { return std::make_shared<T>(); }
    });
}

} // namespace Lvs::Engine::DataModel::ClassRegistry
