#include "Lvs/Engine/Objects/MeshPart.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& MeshPart::Descriptor() {
    static Core::ClassDescriptor descriptor("MeshPart", &BasePart::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "ContentId",
            {},
            true,
            "Appearance",
            {},
            false,
            Core::StringList{"IsPath"},
            Core::HashMap<Core::String, Core::Variant>{
                {"FileExtensions", Core::Variant::From(Core::String(".obj;.fbx;.gltf;.glb;.dae;.3ds;.blend;.stl;.ply"))}
            }
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
