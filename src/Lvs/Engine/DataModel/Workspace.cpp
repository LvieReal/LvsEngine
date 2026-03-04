#include "Lvs/Engine/DataModel/Workspace.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/ServiceRegistry.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Workspace::Descriptor() {
    static Core::ClassDescriptor descriptor("Workspace", &Service::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<std::shared_ptr<Objects::Camera>>(
            "CurrentCamera",
            nullptr,
            true,
            "Data",
            {},
            true,
            {},
            {},
            true
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Workspace>("Workspace", "Services", "Service");
        ServiceRegistry::RegisterService<Workspace>();
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Workspace::Workspace()
    : Service(Descriptor()) {
    SetInsertable(false);
}

void Workspace::InitializeDefaultObjects() {
    auto camera = std::make_shared<Objects::Camera>();
    camera->SetParent(shared_from_this());
    SetProperty("CurrentCamera", QVariant::fromValue(camera));
}

} // namespace Lvs::Engine::DataModel
