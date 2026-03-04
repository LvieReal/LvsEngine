#include "Lvs/Engine/DataModel/Service.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Service::Descriptor() {
    static Core::ClassDescriptor descriptor("Service", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Service>("Service", "DataModel", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Service::Service()
    : Service(Descriptor()) {
}

Service::Service(const Core::ClassDescriptor& descriptor)
    : Core::Instance(descriptor) {
    SetServiceFlags(true, false);
}

} // namespace Lvs::Engine::DataModel
