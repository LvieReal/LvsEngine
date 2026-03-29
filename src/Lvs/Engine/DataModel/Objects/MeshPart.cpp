#include "Lvs/Engine/DataModel/Objects/MeshPart.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& MeshPart::Descriptor() {
    static Core::ClassDescriptor descriptor("MeshPart", &BasePart::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<MeshPart>("MeshPart", "3D Objects", "BasePart");
    });
    return descriptor;
}

MeshPart::MeshPart()
    : BasePart(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects
