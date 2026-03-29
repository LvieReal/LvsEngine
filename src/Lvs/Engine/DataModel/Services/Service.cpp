#include "Lvs/Engine/DataModel/Services/Service.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Service::Descriptor() {
    static Core::ClassDescriptor descriptor("Service", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Service>("Service", "DataModel", "Instance");
    });
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
