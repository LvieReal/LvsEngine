#include "Lvs/Engine/Objects/Model.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& Model::Descriptor() {
    static Core::ClassDescriptor descriptor("Model", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Renders",
            true,
            true,
            "Appearance",
            "When disabled, geometry within this model will not be rendered."
        ));

        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::Variant::InstanceRef>(
            "PrimaryPart",
            {},
            true,
            "Data",
            {},
            false,
            {},
            {},
            true
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<Model>("Model", "Containers", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Model::Model()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects
