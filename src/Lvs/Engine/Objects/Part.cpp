#include "Lvs/Engine/Objects/Part.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& Part::Descriptor() {
    static Core::ClassDescriptor descriptor("Part", &BasePart::Descriptor());
    static const bool initialized = []() {
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartShape>(
            "Shape", Enums::PartShape::Cube, true, "Appearance"
        ));

        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartSurfaceType>(
            "RightSurface", Enums::PartSurfaceType::Smooth, true, "Surfaces"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartSurfaceType>(
            "LeftSurface", Enums::PartSurfaceType::Smooth, true, "Surfaces"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartSurfaceType>(
            "TopSurface", Enums::PartSurfaceType::Studs, true, "Surfaces"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartSurfaceType>(
            "BottomSurface", Enums::PartSurfaceType::Inlets, true, "Surfaces"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartSurfaceType>(
            "FrontSurface", Enums::PartSurfaceType::Smooth, true, "Surfaces"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::PartSurfaceType>(
            "BackSurface", Enums::PartSurfaceType::Smooth, true, "Surfaces"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<Part>("Part", "3D Objects", "BasePart");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Part::Part()
    : Part(Descriptor()) {
}

Part::Part(const Core::ClassDescriptor& descriptor)
    : BasePart(descriptor) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects
