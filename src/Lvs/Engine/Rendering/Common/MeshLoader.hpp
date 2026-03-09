#pragma once

#include "Lvs/Engine/Rendering/Common/MeshData.hpp"

#include <filesystem>
#include <optional>

namespace Lvs::Engine::Rendering::Common {

[[nodiscard]] std::optional<MeshData> LoadMeshFromFile(const std::filesystem::path& filePath, bool smoothNormals);

} // namespace Lvs::Engine::Rendering::Common
