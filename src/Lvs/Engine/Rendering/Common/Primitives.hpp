#pragma once

#include "Lvs/Engine/Rendering/Common/MeshData.hpp"

namespace Lvs::Engine::Rendering::Common::Primitives {

MeshData GenerateCube();
MeshData GenerateBeveledCube(float sizeX, float sizeY, float sizeZ, float bevelWidthWorld, bool smoothNormals);
MeshData GenerateSphere(int rings = 16, int segments = 24);
MeshData GenerateCylinder(int segments = 24, bool caps = true);
MeshData GenerateCone(int segments = 24, bool caps = true);

} // namespace Lvs::Engine::Rendering::Common::Primitives
