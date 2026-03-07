#include "Lvs/Engine/Rendering/Common/Mesh.hpp"

#include "Lvs/Engine/Rendering/Common/Vertex.hpp"

#include <stdexcept>
#include <utility>

namespace Lvs::Engine::Rendering::Common {

Mesh::Mesh(Common::MeshData data)
    : data_(std::move(data)) {
}

void Mesh::EnsureUploaded(Common::GraphicsContext& context) {
    if (vertexBuffer_ != nullptr && indexBuffer_ != nullptr) {
        return;
    }
    if (data_.Vertices.empty() || data_.Indices.empty()) {
        throw std::runtime_error("Mesh has no vertex or index data.");
    }

    const std::size_t vertexSize = data_.Vertices.size() * sizeof(Common::Vertex);
    const std::size_t indexSize = data_.Indices.size() * sizeof(std::uint32_t);

    vertexBuffer_ = context.CreateBuffer(Common::BufferDesc{
        .Size = vertexSize,
        .Usage = Common::BufferUsage::Vertex,
        .Memory = Common::MemoryUsage::CpuVisible
    });
    indexBuffer_ = context.CreateBuffer(Common::BufferDesc{
        .Size = indexSize,
        .Usage = Common::BufferUsage::Index,
        .Memory = Common::MemoryUsage::CpuVisible
    });

    vertexBuffer_->Upload(data_.Vertices.data(), vertexSize);
    indexBuffer_->Upload(data_.Indices.data(), indexSize);
}

void Mesh::Draw(Common::CommandBuffer& commandBuffer) const {
    commandBuffer.BindVertexBuffer(*vertexBuffer_);
    commandBuffer.BindIndexBuffer(*indexBuffer_, Common::IndexFormat::UInt32);
    commandBuffer.DrawIndexed(static_cast<std::uint32_t>(data_.Indices.size()), 1);
}

} // namespace Lvs::Engine::Rendering::Common
