#include "Lvs/Engine/Core/ObjectBase.hpp"

#include <stdexcept>

namespace Lvs::Engine::Core {

ObjectBase::ObjectBase(const ClassDescriptor& descriptor)
    : classDescriptor_(&descriptor) {
    for (const auto& [name, definition] : classDescriptor_->PropertyDefinitions()) {
        properties_.insert({name, Property(definition)});
    }
}

const ClassDescriptor& ObjectBase::GetClassDescriptor() const {
    return *classDescriptor_;
}

const HashMap<String, Property>& ObjectBase::GetProperties() const {
    return properties_;
}

HashMap<String, Property>& ObjectBase::GetProperties() {
    return properties_;
}

Variant ObjectBase::GetProperty(const String& name) const {
    const auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error("No property '" + name + "'");
    }
    return it->second.Get();
}

void ObjectBase::SetProperty(const String& name, const Variant& value) {
    auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error("No property '" + name + "'");
    }
    it->second.Set(value);
}

Property& ObjectBase::GetPropertyObject(const String& name) {
    auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error("No property '" + name + "'");
    }
    return it->second;
}

const Property& ObjectBase::GetPropertyObject(const String& name) const {
    const auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error("No property '" + name + "'");
    }
    return it->second;
}

} // namespace Lvs::Engine::Core
