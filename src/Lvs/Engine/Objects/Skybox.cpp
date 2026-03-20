#include "Lvs/Engine/Objects/Skybox.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/Core/PropertyTags.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

namespace Lvs::Engine::Objects {

Core::ClassDescriptor& Skybox::Descriptor() {
    static Core::ClassDescriptor descriptor("Skybox", &Core::Instance::Descriptor());
    static const bool initialized = []() {
        const Core::String individualVisibleTag = Core::PropertyTags::BuildVisibleIfTag("TextureLayout", "Individual");
        const Core::String crossVisibleTag = Core::PropertyTags::BuildVisibleIfTag("TextureLayout", "Cross");

        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "RightTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullsky/RT.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", individualVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "LeftTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullsky/LF.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", individualVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "UpTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullsky/UP.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", individualVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "DownTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullsky/DN.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", individualVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "FrontTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullsky/FT.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", individualVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "BackTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullsky/BK.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", individualVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Core::String>(
            "CrossTexture",
            Utils::SourcePath::GetResourcePath("Sky/nullcrosssky/sky512.png"),
            true,
            "Textures",
            {},
            false,
            Core::StringList{"IsPath", crossVisibleTag}
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::SkyboxTextureLayout>(
            "TextureLayout",
            Enums::SkyboxTextureLayout::Individual,
            true,
            "Textures"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Math::Color3>(
            "Tint", Math::Color3{1.0, 1.0, 1.0}, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<Enums::TextureFiltering>(
            "Filtering", Enums::TextureFiltering::Linear, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<int>(
            "ResolutionCap", 1024, true, "Appearance"
        ));
        descriptor.RegisterProperty(Core::ObjectBase::MakePropertyDefinition<bool>(
            "Compression", false, true, "Appearance"
        ));

        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        DataModel::ClassRegistry::RegisterClass<Skybox>("Skybox", "Environment", "Instance");
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Skybox::Skybox()
    : Core::Instance(Descriptor()) {
    SetInsertable(true);
}

} // namespace Lvs::Engine::Objects
