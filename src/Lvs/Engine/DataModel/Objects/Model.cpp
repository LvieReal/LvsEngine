#include "Lvs/Engine/DataModel/Objects/Model.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& Model::Descriptor() {
    static Core::ClassDescriptor descriptor("Model", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Model>("Model", "Containers", "Instance");
    });
    return descriptor;
}

Model::Model()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects
