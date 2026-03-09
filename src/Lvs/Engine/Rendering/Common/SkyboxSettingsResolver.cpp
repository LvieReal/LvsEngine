#include "Lvs/Engine/Rendering/Common/SkyboxSettingsResolver.hpp"

#include "Lvs/Engine/Objects/Skybox.hpp"
#include "Lvs/Engine/Rendering/Common/EnvironmentLookup.hpp"

namespace Lvs::Engine::Rendering::Common {

SkyboxSettingsSnapshot SkyboxSettingsResolver::Resolve(const std::shared_ptr<DataModel::Place>& place) const {
    SkyboxSettingsSnapshot snapshot{};
    snapshot.Source = FindSkyboxInstance(FindLightingService(place));
    const auto defaults = std::make_shared<Objects::Skybox>();
    const std::shared_ptr<Objects::Skybox> source = snapshot.Source != nullptr ? snapshot.Source : defaults;

    const auto resolvePathOrDefault = [&source, &defaults](const char* propertyName) {
        const auto value = source->GetProperty(propertyName).toString().trimmed().toStdString();
        if (!value.empty()) {
            return value;
        }
        return defaults->GetProperty(propertyName).toString().trimmed().toStdString();
    };

    snapshot.Faces = {
        resolvePathOrDefault("RightTexture"),
        resolvePathOrDefault("LeftTexture"),
        resolvePathOrDefault("UpTexture"),
        resolvePathOrDefault("DownTexture"),
        resolvePathOrDefault("FrontTexture"),
        resolvePathOrDefault("BackTexture")
    };
    snapshot.Tint = source->GetProperty("Tint").value<Math::Color3>();
    snapshot.Filtering = source->GetProperty("Filtering").value<Enums::TextureFiltering>();
    snapshot.TextureLayout = source->GetProperty("TextureLayout").value<Enums::SkyboxTextureLayout>();
    snapshot.CrossTexture = resolvePathOrDefault("CrossTexture");
    snapshot.ResolutionCap = source->GetProperty("ResolutionCap").toInt();
    snapshot.Compression = source->GetProperty("Compression").toBool();
    return snapshot;
}

} // namespace Lvs::Engine::Rendering::Common
