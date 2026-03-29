#include "Lvs/Engine/Reflection/ReflectionSystem.hpp"

#include "Lvs/Engine/Core/ExternalMetadata.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Services/Lighting.hpp"
#include "Lvs/Engine/DataModel/Services/QualitySettings.hpp"
#include "Lvs/Engine/DataModel/Services/Selection.hpp"
#include "Lvs/Engine/DataModel/Services/Service.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/DataModel/Objects/BasePart.hpp"
#include "Lvs/Engine/DataModel/Objects/Camera.hpp"
#include "Lvs/Engine/DataModel/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/DataModel/Objects/Folder.hpp"
#include "Lvs/Engine/DataModel/Objects/Light.hpp"
#include "Lvs/Engine/DataModel/Objects/MeshPart.hpp"
#include "Lvs/Engine/DataModel/Objects/Model.hpp"
#include "Lvs/Engine/DataModel/Objects/Part.hpp"
#include "Lvs/Engine/DataModel/Objects/PostEffects.hpp"
#include "Lvs/Engine/DataModel/Objects/SelectionBox.hpp"
#include "Lvs/Engine/DataModel/Objects/Skybox.hpp"

#include <mutex>

namespace Lvs::Engine::Reflection {

void EnsureInitialized() {
    static std::once_flag once;
    std::call_once(once, []() {
        // Core/base descriptors.
        (void)Core::Instance::Descriptor();

        // DataModel + services.
        (void)DataModel::DataModel::Descriptor();
        (void)DataModel::Service::Descriptor();
        (void)DataModel::Workspace::Descriptor();
        (void)DataModel::Lighting::Descriptor();
        (void)DataModel::QualitySettings::Descriptor();
        (void)DataModel::Selection::Descriptor();
        (void)DataModel::ChangeHistoryService::Descriptor();

        // Objects.
        (void)DataModel::Objects::BasePart::Descriptor();
        (void)DataModel::Objects::Part::Descriptor();
        (void)DataModel::Objects::MeshPart::Descriptor();
        (void)DataModel::Objects::Camera::Descriptor();
        (void)DataModel::Objects::Light::Descriptor();
        (void)DataModel::Objects::DirectionalLight::Descriptor();
        (void)DataModel::Objects::PostEffects::Descriptor();
        (void)DataModel::Objects::Skybox::Descriptor();
        (void)DataModel::Objects::SelectionBox::Descriptor();
        (void)DataModel::Objects::Model::Descriptor();
        (void)DataModel::Objects::Folder::Descriptor();

        // Load+apply external metadata once descriptors exist.
        Core::ExternalMetadata::Get().EnsureLoaded();
    });
}

} // namespace Lvs::Engine::Reflection
