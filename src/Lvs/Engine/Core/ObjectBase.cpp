#include "Lvs/Engine/Core/ObjectBase.hpp"

#include <stdexcept>

namespace Lvs::Engine::Core {

ObjectBase::ObjectBase(const ClassDescriptor& descriptor)
    : classDescriptor_(&descriptor) {
    for (auto it = classDescriptor_->PropertyDefinitions().cbegin(); it != classDescriptor_->PropertyDefinitions().cend(); ++it) {
        properties_.insert(it.key(), Property(it.value()));
    }
}

const ClassDescriptor& ObjectBase::GetClassDescriptor() const {
    return *classDescriptor_;
}

const QMap<QString, Property>& ObjectBase::GetProperties() const {
    return properties_;
}

QMap<QString, Property>& ObjectBase::GetProperties() {
    return properties_;
}

QVariant ObjectBase::GetProperty(const QString& name) const {
    const auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error(QString("No property '%1'").arg(name).toStdString());
    }
    return it->Get();
}

void ObjectBase::SetProperty(const QString& name, const QVariant& value) {
    auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error(QString("No property '%1'").arg(name).toStdString());
    }
    it->Set(value);
}

Property& ObjectBase::GetPropertyObject(const QString& name) {
    auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error(QString("No property '%1'").arg(name).toStdString());
    }
    return *it;
}

const Property& ObjectBase::GetPropertyObject(const QString& name) const {
    const auto it = properties_.find(name);
    if (it == properties_.end()) {
        throw std::runtime_error(QString("No property '%1'").arg(name).toStdString());
    }
    return *it;
}

} // namespace Lvs::Engine::Core
