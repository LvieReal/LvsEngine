#include "Lvs/Engine/Rendering/Backends/OpenGL/GLCommandBuffer.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"

namespace Lvs::Engine::Rendering::Backends::OpenGL {

GLCommandBuffer::GLCommandBuffer(GLContext& context)
    : context_(context) {}

void GLCommandBuffer::BeginRenderPass(const RHI::RenderPassInfo& info) {
    context_.BeginRenderPass(info);
}

void GLCommandBuffer::EndRenderPass() {
    context_.EndRenderPass();
}

void GLCommandBuffer::BindPipeline(const RHI::IPipeline& pipeline) {
    context_.BindPipeline(pipeline);
}

void GLCommandBuffer::BindVertexBuffer(const RHI::u32 slot, const RHI::IBuffer& buffer, const std::size_t offset) {
    context_.BindVertexBuffer(slot, buffer, offset);
}

void GLCommandBuffer::BindIndexBuffer(const RHI::IBuffer& buffer, const RHI::IndexType indexType, const std::size_t offset) {
    context_.BindIndexBuffer(buffer, indexType, offset);
}

void GLCommandBuffer::BindResourceSet(const RHI::u32 slot, const RHI::IResourceSet& set) {
    context_.BindResourceSet(slot, set);
}

void GLCommandBuffer::PushConstants(const void* data, const std::size_t size) {
    context_.PushConstants(data, size);
}

void GLCommandBuffer::DrawIndexed(const RHI::u32 indexCount) {
    context_.DrawIndexed(indexCount);
}

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
