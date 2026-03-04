#pragma once

#include "Lvs/Engine/Rendering/Vulkan/MeshData.hpp"

namespace Lvs::Engine::Rendering::Vulkan::Primitives {

MeshData GenerateCube();
MeshData GenerateSphere(int rings = 16, int segments = 24);
MeshData GenerateCylinder(int segments = 24, bool caps = true);
MeshData GenerateCone(int segments = 24, bool caps = true);

} // namespace Lvs::Engine::Rendering::Vulkan::Primitives
