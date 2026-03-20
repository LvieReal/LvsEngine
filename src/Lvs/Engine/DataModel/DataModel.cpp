#include "Lvs/Engine/DataModel/DataModel.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& DataModel::Descriptor() {
    static Core::ClassDescriptor descriptor("DataModel", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<DataModel>("DataModel", "DataModel", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

DataModel::DataModel()
    : Core::Instance(Descriptor()) {
    SetInsertable(false);
}

void DataModel::RegisterInstance(const std::shared_ptr<Core::Instance>& instance) {
    instanceRegistry_[instance->GetId()] = instance;
}

void DataModel::UnregisterInstance(const std::shared_ptr<Core::Instance>& instance) {
    instanceRegistry_.erase(instance->GetId());
}

std::shared_ptr<Core::Instance> DataModel::FindInstanceById(const Core::String& instanceId) const {
    const auto it = instanceRegistry_.find(instanceId);
    if (it == instanceRegistry_.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void DataModel::SetOwnerPlace(Place* ownerPlace) {
    ownerPlace_ = ownerPlace;
}

Place* DataModel::GetOwnerPlace() const {
    return ownerPlace_;
}

} // namespace Lvs::Engine::DataModel
