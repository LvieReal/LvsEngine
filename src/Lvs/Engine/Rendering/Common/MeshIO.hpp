#pragma once

#include "Lvs/Engine/Rendering/Common/MeshData.hpp"

#include <filesystem>

namespace Lvs::Engine::Rendering::Common {

MeshData LoadObjMesh(const std::filesystem::path& path);

} // namespace Lvs::Engine::Rendering::Common
