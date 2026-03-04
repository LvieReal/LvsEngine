#pragma once

#include "Lvs/Engine/Core/PropertyDefinition.hpp"

#include <QMap>
#include <QString>

namespace Lvs::Engine::Core {

class ClassDescriptor {
public:
    ClassDescriptor(QString className, const ClassDescriptor* baseDescriptor = nullptr);

    void RegisterProperty(const PropertyDefinition& propertyDefinition);

    [[nodiscard]] const QString& ClassName() const;
    [[nodiscard]] const ClassDescriptor* BaseDescriptor() const;
    [[nodiscard]] const QMap<QString, PropertyDefinition>& PropertyDefinitions() const;

    static void RegisterClassDescriptor(const ClassDescriptor* descriptor);
    static const ClassDescriptor* Get(const QString& className);

private:
    QString className_;
    const ClassDescriptor* baseDescriptor_{nullptr};
    QMap<QString, PropertyDefinition> propertyDefinitions_;
    int nextPropertyOrder_{0};
};

} // namespace Lvs::Engine::Core
