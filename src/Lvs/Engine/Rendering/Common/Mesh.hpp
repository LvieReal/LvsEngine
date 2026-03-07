#pragma once

#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/GraphicsContext.hpp"
#include "Lvs/Engine/Rendering/Common/MeshData.hpp"
#include "Lvs/Engine/Rendering/Common/MeshUploader.hpp"

#include <memory>

namespace Lvs::Engine::Rendering::Common {

class Mesh final : public Common::UploadedMesh {
public:
    explicit Mesh(Common::MeshData data);
    ~Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(Mesh&&) = delete;

    void EnsureUploaded(Common::GraphicsContext& context);
    void Draw(Common::CommandBuffer& commandBuffer) const;

private:
    Common::MeshData data_;
    std::unique_ptr<Common::BufferResource> vertexBuffer_;
    std::unique_ptr<Common::BufferResource> indexBuffer_;
};

} // namespace Lvs::Engine::Rendering::Common
