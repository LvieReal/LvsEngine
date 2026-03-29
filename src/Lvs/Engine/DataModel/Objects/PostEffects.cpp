#include "Lvs/Engine/DataModel/Objects/PostEffects.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Math/Color3.hpp"

#include <mutex>

namespace Lvs::Engine::DataModel::Objects {

Core::ClassDescriptor& PostEffects::Descriptor() {
    static Core::ClassDescriptor descriptor("PostEffects", &Core::Instance::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<PostEffects>("PostEffects", "Rendering", "Instance");
    });
    return descriptor;
}

PostEffects::PostEffects()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::DataModel::Objects
