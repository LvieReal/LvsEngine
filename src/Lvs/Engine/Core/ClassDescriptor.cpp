#include "Lvs/Engine/Core/ClassDescriptor.hpp"

#include <QHash>

#include <algorithm>

namespace Lvs::Engine::Core {

namespace {
QHash<QString, const ClassDescriptor*>& Registry() {
    static QHash<QString, const ClassDescriptor*> registry;
    return registry;
}
} // namespace

ClassDescriptor::ClassDescriptor(QString className, const ClassDescriptor* baseDescriptor)
    : className_(std::move(className)),
      baseDescriptor_(baseDescriptor) {
    if (baseDescriptor_ != nullptr) {
        propertyDefinitions_ = baseDescriptor_->PropertyDefinitions();
        for (auto it = propertyDefinitions_.cbegin(); it != propertyDefinitions_.cend(); ++it) {
            nextPropertyOrder_ = std::max(nextPropertyOrder_, it->RegistrationOrder + 1);
        }
    }
}

void ClassDescriptor::RegisterProperty(const PropertyDefinition& propertyDefinition) {
    PropertyDefinition definition = propertyDefinition;
    const auto existing = propertyDefinitions_.find(definition.Name);
    if (existing != propertyDefinitions_.end() && existing->RegistrationOrder >= 0) {
        definition.RegistrationOrder = existing->RegistrationOrder;
    } else if (definition.RegistrationOrder < 0) {
        definition.RegistrationOrder = nextPropertyOrder_++;
    }
    propertyDefinitions_[definition.Name] = definition;
}

const QString& ClassDescriptor::ClassName() const {
    return className_;
}

const ClassDescriptor* ClassDescriptor::BaseDescriptor() const {
    return baseDescriptor_;
}

const QMap<QString, PropertyDefinition>& ClassDescriptor::PropertyDefinitions() const {
    return propertyDefinitions_;
}

void ClassDescriptor::RegisterClassDescriptor(const ClassDescriptor* descriptor) {
    Registry().insert(descriptor->ClassName(), descriptor);
}

const ClassDescriptor* ClassDescriptor::Get(const QString& className) {
    return Registry().value(className, nullptr);
}

} // namespace Lvs::Engine::Core
