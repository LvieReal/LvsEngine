#include "Lvs/Engine/DataModel/Objects/SelectionBox.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& SelectionBox::Descriptor() {
    static Core::ClassDescriptor descriptor("SelectionBox", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<SelectionBox>("SelectionBox", "Visuals", "Instance");
    });
    return descriptor;
}

SelectionBox::SelectionBox()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects
