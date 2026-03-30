#include "Lvs/Engine/DataModel/Objects/Image3D.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& Image3D::Descriptor() {
    static Core::ClassDescriptor descriptor("Image3D", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Image3D>("Image3D", "Display", "Instance");
    });
    return descriptor;
}

Image3D::Image3D()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects

