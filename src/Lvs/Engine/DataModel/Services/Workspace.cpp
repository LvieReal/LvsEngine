#include "Lvs/Engine/DataModel/Services/Workspace.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"
#include "Lvs/Engine/DataModel/Objects/Camera.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Workspace::Descriptor() {
    static Core::ClassDescriptor descriptor("Workspace", &Service::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Workspace>("Workspace", "Services", "Service");
        ServiceRegistry::RegisterService<Workspace>();
    });
    return descriptor;
}

Workspace::Workspace()
    : Service(Descriptor()) {
    SetInsertable(false);
}

void Workspace::InitializeDefaultObjects() {
    auto camera = std::make_shared<Objects::Camera>();
    camera->SetParent(shared_from_this());
    SetProperty("CurrentCamera", Core::Variant::InstanceRef{camera});
}

} // namespace Lvs::Engine::DataModel
