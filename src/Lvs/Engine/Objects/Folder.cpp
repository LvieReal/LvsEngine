#include "Lvs/Engine/Objects/Folder.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& Folder::Descriptor() {
    static Core::ClassDescriptor descriptor("Folder", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<Folder>("Folder", "Containers", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Folder::Folder()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects

