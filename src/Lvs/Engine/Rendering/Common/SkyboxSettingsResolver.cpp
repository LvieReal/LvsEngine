#include "Lvs/Engine/Rendering/Common/SkyboxSettingsResolver.hpp"

#include "Lvs/Engine/Rendering/Common/EnvironmentLookup.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"

namespace Lvs::Engine::Rendering::Common {

SkyboxSettingsSnapshot SkyboxSettingsResolver::Resolve(const std::shared_ptr<DataModel::Place>& place) const {
    SkyboxSettingsSnapshot snapshot{};
    snapshot.Source = FindSkyboxInstance(FindLightingService(place));
    std::shared_ptr<Objects::Skybox> source = snapshot.Source;
    if (source == nullptr) {
        source = std::make_shared<Objects::Skybox>();
    }

    snapshot.Faces = {
        source->GetProperty("RightTexture").toString().trimmed().toStdString(),
        source->GetProperty("LeftTexture").toString().trimmed().toStdString(),
        source->GetProperty("UpTexture").toString().trimmed().toStdString(),
        source->GetProperty("DownTexture").toString().trimmed().toStdString(),
        source->GetProperty("FrontTexture").toString().trimmed().toStdString(),
        source->GetProperty("BackTexture").toString().trimmed().toStdString()
    };
    snapshot.Tint = source->GetProperty("Tint").value<Math::Color3>();
    snapshot.Filtering = source->GetProperty("Filtering").value<Enums::TextureFiltering>();
    snapshot.TextureLayout = source->GetProperty("TextureLayout").value<Enums::SkyboxTextureLayout>();
    snapshot.CrossTexture = source->GetProperty("CrossTexture").toString().trimmed().toStdString();
    snapshot.ResolutionCap = source->GetProperty("ResolutionCap").toInt();
    snapshot.Compression = source->GetProperty("Compression").toBool();
    return snapshot;
}

} // namespace Lvs::Engine::Rendering::Common

