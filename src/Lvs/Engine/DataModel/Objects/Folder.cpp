#include "Lvs/Engine/DataModel/Objects/Folder.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& Folder::Descriptor() {
    static Core::ClassDescriptor descriptor("Folder", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Folder>("Folder", "Containers", "Instance");
    });
    return descriptor;
}

Folder::Folder()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects
