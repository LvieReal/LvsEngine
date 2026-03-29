#include "Lvs/Engine/Rendering/Common/SkyboxSettingsResolver.hpp"

#include "Lvs/Engine/DataModel/Objects/Skybox.hpp"
#include "Lvs/Engine/Rendering/Common/EnvironmentLookup.hpp"

#include <cctype>

namespace Lvs::Engine::Rendering::Common {

SkyboxSettingsSnapshot SkyboxSettingsResolver::Resolve(const std::shared_ptr<DataModel::Place>& place) const {
    SkyboxSettingsSnapshot snapshot{};
    snapshot.Source = FindSkyboxInstance(FindLightingService(place));
    const auto defaults = std::make_shared<DataModel::Objects::Skybox>();
    const std::shared_ptr<DataModel::Objects::Skybox> source = snapshot.Source != nullptr ? snapshot.Source : defaults;

    const auto resolvePathOrDefault = [&source, &defaults](const char* propertyName) -> std::filesystem::path {
        auto trimCopy = [](const std::string& in) -> std::string {
            size_t start = 0;
            size_t end = in.size();
            while (start < end && std::isspace(static_cast<unsigned char>(in[start])) != 0) {
                ++start;
            }
            while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1])) != 0) {
                --end;
            }
            return in.substr(start, end - start);
        };

        const std::string raw = trimCopy(source->GetProperty(propertyName).toString());
        if (!raw.empty()) {
            return std::filesystem::path(raw);
        }
        return std::filesystem::path(trimCopy(defaults->GetProperty(propertyName).toString()));
    };

    snapshot.Faces = std::array<std::filesystem::path, 6>{
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
