#pragma once

#include "Lvs/Engine/Core/ClassDescriptor.hpp"
#include "Lvs/Engine/Core/EnumTraits.hpp"
#include "Lvs/Engine/Core/Property.hpp"
#include "Lvs/Engine/Core/TypeId.hpp"
#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Core/Variant.hpp"

namespace Lvs::Engine::Core {

class ObjectBase {
public:
    explicit ObjectBase(const ClassDescriptor& descriptor);
    virtual ~ObjectBase() = default;

    template <typename T>
    static PropertyDefinition MakePropertyDefinition(
        const String& name,
        const T& defaultValue = T{},
        const bool serializable = true,
        const String& category = "Data",
        const String& description = {},
        const bool readOnly = false,
        const StringList& customTags = {},
        const HashMap<String, Variant>& customAttributes = {},
        const bool isInstanceReference = false
    ) {
        TypeId typeId = TypeIdOf<T>();
        HashMap<String, Variant> attributes = customAttributes;
        if constexpr (std::is_enum_v<T>) {
            typeId = TypeId::Enum;
            if constexpr (HasEnumName<T>()) {
                attributes.insert_or_assign("EnumType", Variant::From(String(EnumTraits<T>::Name)));
            }
        }
        return PropertyDefinition{
            .Name = name,
            .Type = typeId,
            .Default = Variant::From(defaultValue),
            .Serializable = serializable,
            .Category = category,
            .Description = description,
            .ReadOnly = readOnly,
            .CustomTags = customTags,
            .CustomAttributes = std::move(attributes),
            .IsInstanceReference = isInstanceReference
        };
    }

    [[nodiscard]] const ClassDescriptor& GetClassDescriptor() const;
    [[nodiscard]] const HashMap<String, Property>& GetProperties() const;
    [[nodiscard]] HashMap<String, Property>& GetProperties();
    [[nodiscard]] Variant GetProperty(const String& name) const;
    void SetProperty(const String& name, const Variant& value);
    Property& GetPropertyObject(const String& name);
    const Property& GetPropertyObject(const String& name) const;
    void SyncPropertiesFromDescriptor(bool preserveValues = true);

private:
    const ClassDescriptor* classDescriptor_{nullptr};
    HashMap<String, Property> properties_;
};

} // namespace Lvs::Engine::Core
