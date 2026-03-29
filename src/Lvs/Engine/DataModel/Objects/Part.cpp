#include "Lvs/Engine/DataModel/Objects/Part.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& Part::Descriptor() {
    static Core::ClassDescriptor descriptor("Part", &BasePart::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Part>("Part", "3D Objects", "BasePart");
    });
    return descriptor;
}

Part::Part()
    : Part(Descriptor()) {
}

Part::Part(const Core::ClassDescriptor& descriptor)
    : BasePart(descriptor) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects
