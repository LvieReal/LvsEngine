#pragma once

#include "Lvs/Engine/Core/PropertyDefinition.hpp"
#include "Lvs/Engine/Core/Types.hpp"

namespace Lvs::Engine::Core {

class ClassDescriptor {
public:
    ClassDescriptor(String className, const ClassDescriptor* baseDescriptor = nullptr);

    void RegisterProperty(const PropertyDefinition& propertyDefinition);
    void ResetPropertiesToBase();

    [[nodiscard]] const String& ClassName() const;
    [[nodiscard]] const ClassDescriptor* BaseDescriptor() const;
    [[nodiscard]] const HashMap<String, PropertyDefinition>& PropertyDefinitions() const;

    static void RegisterClassDescriptor(const ClassDescriptor* descriptor);
    static const ClassDescriptor* Get(const String& className);
    static Vector<const ClassDescriptor*> GetAll();

private:
    String className_;
    const ClassDescriptor* baseDescriptor_{nullptr};
    HashMap<String, PropertyDefinition> propertyDefinitions_;
    int nextPropertyOrder_{0};
};

} // namespace Lvs::Engine::Core
