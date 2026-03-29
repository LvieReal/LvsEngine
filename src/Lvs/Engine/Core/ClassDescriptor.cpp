#include "Lvs/Engine/Core/ClassDescriptor.hpp"

#include <algorithm>
#include <unordered_map>

namespace Lvs::Engine::Core {

namespace {
std::unordered_map<String, const ClassDescriptor*>& Registry() {
    static std::unordered_map<String, const ClassDescriptor*> registry;
    return registry;
}
} // namespace

ClassDescriptor::ClassDescriptor(String className, const ClassDescriptor* baseDescriptor)
    : className_(std::move(className)),
      baseDescriptor_(baseDescriptor) {
    if (baseDescriptor_ != nullptr) {
        propertyDefinitions_ = baseDescriptor_->PropertyDefinitions();
        for (const auto& [_, def] : propertyDefinitions_) {
            nextPropertyOrder_ = std::max(nextPropertyOrder_, def.RegistrationOrder + 1);
        }
    }
}

void ClassDescriptor::RegisterProperty(const PropertyDefinition& propertyDefinition) {
    PropertyDefinition definition = propertyDefinition;
    const auto existing = propertyDefinitions_.find(definition.Name);
    if (existing != propertyDefinitions_.end() && existing->second.RegistrationOrder >= 0) {
        definition.RegistrationOrder = existing->second.RegistrationOrder;
    } else if (definition.RegistrationOrder < 0) {
        definition.RegistrationOrder = nextPropertyOrder_++;
    }
    propertyDefinitions_[definition.Name] = definition;
    if (definition.RegistrationOrder >= 0) {
        nextPropertyOrder_ = std::max(nextPropertyOrder_, definition.RegistrationOrder + 1);
    }
}

void ClassDescriptor::ResetPropertiesToBase() {
    propertyDefinitions_.clear();
    nextPropertyOrder_ = 0;
    if (baseDescriptor_ == nullptr) {
        return;
    }
    propertyDefinitions_ = baseDescriptor_->PropertyDefinitions();
    for (const auto& [_, def] : propertyDefinitions_) {
        nextPropertyOrder_ = std::max(nextPropertyOrder_, def.RegistrationOrder + 1);
    }
}

const String& ClassDescriptor::ClassName() const {
    return className_;
}

const ClassDescriptor* ClassDescriptor::BaseDescriptor() const {
    return baseDescriptor_;
}

const HashMap<String, PropertyDefinition>& ClassDescriptor::PropertyDefinitions() const {
    return propertyDefinitions_;
}

void ClassDescriptor::RegisterClassDescriptor(const ClassDescriptor* descriptor) {
    Registry().insert({descriptor->ClassName(), descriptor});
}

const ClassDescriptor* ClassDescriptor::Get(const String& className) {
    const auto it = Registry().find(className);
    if (it == Registry().end()) {
        return nullptr;
    }
    return it->second;
}

Vector<const ClassDescriptor*> ClassDescriptor::GetAll() {
    Vector<const ClassDescriptor*> out;
    out.reserve(Registry().size());
    for (const auto& [_, descriptor] : Registry()) {
        out.push_back(descriptor);
    }
    return out;
}

} // namespace Lvs::Engine::Core
