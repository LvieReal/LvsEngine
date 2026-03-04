#pragma once

#include "Lvs/Engine/Core/ClassDescriptor.hpp"
#include "Lvs/Engine/Core/Property.hpp"

#include <QMap>
#include <QString>
#include <QVariant>

namespace Lvs::Engine::Core {

class ObjectBase {
public:
    explicit ObjectBase(const ClassDescriptor& descriptor);
    virtual ~ObjectBase() = default;

    template <typename T>
    static PropertyDefinition MakePropertyDefinition(
        const QString& name,
        const T& defaultValue = T{},
        const bool serializable = true,
        const QString& category = "Data",
        const QString& description = {},
        const bool readOnly = false,
        const QStringList& customTags = {},
        const QVariantMap& customAttributes = {},
        const bool isInstanceReference = false
    ) {
        return PropertyDefinition{
            .Name = name,
            .Type = QMetaType::fromType<T>(),
            .Default = QVariant::fromValue(defaultValue),
            .Serializable = serializable,
            .Category = category,
            .Description = description,
            .ReadOnly = readOnly,
            .CustomTags = customTags,
            .CustomAttributes = customAttributes,
            .IsInstanceReference = isInstanceReference
        };
    }

    [[nodiscard]] const ClassDescriptor& GetClassDescriptor() const;
    [[nodiscard]] const QMap<QString, Property>& GetProperties() const;
    [[nodiscard]] QMap<QString, Property>& GetProperties();
    [[nodiscard]] QVariant GetProperty(const QString& name) const;
    void SetProperty(const QString& name, const QVariant& value);
    Property& GetPropertyObject(const QString& name);
    const Property& GetPropertyObject(const QString& name) const;

private:
    const ClassDescriptor* classDescriptor_{nullptr};
    QMap<QString, Property> properties_;
};

} // namespace Lvs::Engine::Core
