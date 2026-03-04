#include "Lvs/Engine/Objects/MeshPart.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& MeshPart::Descriptor() {
    static Core::ClassDescriptor descriptor("MeshPart", &BasePart::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<QString>(
            "ContentId",
            {},
            true,
            "Appearance",
            {},
            false,
            {"IsPath"},
            {{"FileExtensions", QStringList{".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".stl", ".ply"}}}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "SmoothNormals", true, true, "Appearance"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<MeshPart>("MeshPart", "3D Objects", "BasePart");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

MeshPart::MeshPart()
    : BasePart(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects
