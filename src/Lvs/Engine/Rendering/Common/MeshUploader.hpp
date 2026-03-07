#pragma once

#include "Lvs/Engine/Rendering/Common/MeshData.hpp"

#include <memory>

namespace Lvs::Engine::Rendering::Common {

class UploadedMesh {
public:
    virtual ~UploadedMesh() = default;
};

class MeshUploader {
public:
    virtual ~MeshUploader() = default;
    [[nodiscard]] virtual std::shared_ptr<UploadedMesh> Upload(MeshData data) = 0;
};

} // namespace Lvs::Engine::Rendering::Common
